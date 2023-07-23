"""inference modules"""
import miniosl
import torch
import numpy as np
import logging
import abc
from typing import Tuple

feature_channels = len(miniosl.channel_id)


def sort_moves(moves, policy):
    flatten = policy.flatten()
    prob = [(flatten[move.policy_move_label()], move) for move in moves]
    prob.sort(key=lambda e: -e[0])
    return prob


def softmax(x):
    b = np.max(x)
    y = np.exp(x - b)
    return y / y.sum()


class InferenceModel(abc.ABC):
    """interface for inference using trained models"""
    def __init__(self):
        super().__init__()

    @abc.abstractmethod
    def infer(self, inputs: torch.Tensor) -> Tuple[np.ndarray, float,
                                                   np.ndarray]:
        """return inference results for batch"""
        pass

    def infer_one(self, input: np.ndarray) -> Tuple[np.ndarray, float,
                                                    np.ndarray]:
        """return inference results for a single instance"""
        outputs = self.infer(torch.stack((torch.from_numpy(input),)))
        return [_[0] for _ in outputs]

    def eval(self, input: np.ndarray, *, take_softmax: bool = False
             ) -> Tuple[np.ndarray, float, np.ndarray]:
        """return (move, value, aux) tuple, after softmax"""
        out = self.infer_one(input)
        moves = softmax(out[0]).reshape(-1, 9, 9) \
            if take_softmax else out[0].reshape(-1, 9, 9)
        value, aux = (out[1].item(), out[2].reshape(-1, 9, 9))
        return (moves, value, aux)


class OnnxInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        super().__init__()
        import onnxruntime as ort
        if device == 'cpu' or not device:
            provider = ['CPUExecutionProvider']
        else:
            provider = ort.get_available_providers()
        self.ort_session = ort.InferenceSession(path, providers=provider)
        logging.info(self.ort_session.get_providers())

    def infer(self, inputs):
        out = self.ort_session.run(None, {"input": inputs.to('cpu').numpy()})
        return out


class TorchTRTInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        import torch_tensorrt
        super().__init__()
        with torch_tensorrt.logging.info():
            self.trt_module = torch.jit.load(path)
        self.device = device

    def infer(self, inputs):
        tensor = inputs.half().to(self.device)
        outputs = self.trt_module(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


class TorchScriptInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        super().__init__()
        self.ts_module = torch.jit.load(path)
        self.device = device

    def infer(self, inputs):
        with torch.no_grad():
            tensor = inputs.to(self.device)
            outputs = self.ts_module(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


class TorchInfer(InferenceModel):
    def __init__(self, model, device: str):
        super().__init__()
        self.model = model
        self.model.eval()
        self.device = device

    def infer(self, inputs):
        with torch.no_grad():
            tensor = inputs.to(self.device)
            outputs = self.model(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


def load(path: str, device: str = "", torch_cfg: dict = {}) -> InferenceModel:
    """factory method to load a model from file

    :param path: filepath,
    :param device: torch device such as 'cuda', 'cpu',
    :param torch_cfg: network specification needed for TorchInfer.
    """
    if path.endswith('.onnx'):
        return OnnxInfer(path, device)
    if path.endswith('.ts'):
        if not device:
            device = 'cuda'
        return TorchTRTInfer(path, device)
    if path.endswith('.pts'):  # need to used a different extention from TRT's
        return TorchScriptInfer(path, device)
    if path.endswith('.pt'):
        model = miniosl.network.StandardNetwork(**torch_cfg).to(device)
        saved_state = torch.load(path, map_location=torch.device(device))
        model.load_state_dict(saved_state)
        return TorchInfer(model, device)
    if path.endswith('.chpt'):
        checkpoint = torch.load(path, map_location=torch.device(device))
        cfg = checkpoint['cfg']
        network_cfg = cfg['network_cfg']
        model = miniosl.StandardNetwork(**network_cfg).to(device)
        model.load_state_dict(checkpoint['model_state_dict'])
        return TorchInfer(model, device)
    raise ValueError("unknown filetype")


class InferenceForGameArray(miniosl.InferenceModelStub):
    def __init__(self, module: InferenceModel):
        super().__init__()
        self.module = module

    def py_infer(self, features):
        features = features.reshape(-1, len(miniosl.channel_id), 9, 9)
        return self.module.infer(torch.from_numpy(features))


def export_onnx(model, *, device, filename):
    import torch.onnx
    model.eval()
    dtype = torch.float
    dummy_input = torch.randn(16, feature_channels, 9, 9, device=device,
                              dtype=dtype)
    if not filename.endswith('.onnx'):
        filename = f'{filename}.onnx'

    torch.onnx.export(model, dummy_input, filename,
                      dynamic_axes={'input': {0: 'batch_size'},
                                    'move': {0: 'batch_size'},
                                    'aux': {0: 'batch_size'}},
                      verbose=True, input_names=['input'],
                      output_names=['move', 'value', 'aux'])


def export_tensorrt(model, savefile):
    import torch_tensorrt
    model.eval()
    model = model.half()
    inputs = [
        torch_tensorrt.Input(
            min_shape=[1, feature_channels, 9, 9],
            opt_shape=[128, feature_channels, 9, 9],
            max_shape=[2048, feature_channels, 9, 9],
            dtype=torch.half)]
    enabled_precisions = {torch.half}
    trt_ts_module = torch_tensorrt.compile(
        model, inputs=inputs, enabled_precisions=enabled_precisions
    )
    input_data = torch.randn(16, feature_channels, 9, 9, device='cuda')
    _ = trt_ts_module(input_data.half())
    filename = savefile if savefile.endswith('.ts') else f'{savefile}.ts'
    torch.jit.save(trt_ts_module, filename)


def export_torch_script(model, *, device, filename):
    model.eval()
    if device == 'cuda':
        inputs = torch.rand(8, feature_channels, 9, 9).to(device)
        ts_module = torch.jit.trace(model, inputs)
    else:
        ts_module = torch.jit.script(model)

    if not filename.endswith('.pts'):
        filename = f'{filename}.pts'
    torch.jit.save(ts_module, filename)


def export_model(model, *, device, filename):
    if filename.endswith('.onnx'):
        export_onnx(model, device=device, filename=filename)
    elif filename.endswith('.ts'):
        export_tensorrt(model, filename)
    elif filename.endswith('.pts'):
        export_torch_script(model, device=device, filename=filename)
    else:
        raise ValueError("unknown filetype")
