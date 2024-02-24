"""inference modules"""
import miniosl
import torch
import numpy as np
import logging
import abc
from typing import Tuple

feature_channels = len(miniosl.channel_id)


def p2elo(p, eps=1e-4):
    return -400 * np.log10(1/(p+eps)-1)


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
    def __init__(self, device):
        super().__init__()
        self.device = device

    @abc.abstractmethod
    def infer(self, inputs: torch.Tensor) -> Tuple[np.ndarray, np.ndarray,
                                                   np.ndarray]:
        """return inference results for batch"""
        pass

    def infer_int8(self, inputs: np.ndarray | torch.Tensor
                   ) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """an optimized path"""
        if isinstance(inputs, np.ndarray):
            if inputs.dtype != np.int8:
                raise ValueError(f'expected int8, got {inputs.dtype}')
            inputs = torch.from_numpy(inputs)
        if inputs.dtype != torch.int8:
            raise ValueError(f'expected int8, got {inputs.dtype}')
        inputs = inputs.to(self.device).float()
        inputs /= miniosl.One
        return self.infer(inputs)

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
        import re
        super().__init__(device)
        import onnxruntime as ort
        if device == 'cpu':
            provider = ['CPUExecutionProvider']
        elif device.startswith('cuda'):
            cuda_pattern = r'^cuda:([0-9]+)$'
            if match := re.match(cuda_pattern, device):
                provider = [('CUDAExecutionProvider',
                             {'device_id': int(match.group(1))}),
                            'CPUExecutionProvider']
            else:
                provider = ['CUDAExecutionProvider', 'CPUExecutionProvider']
        else:
            provider = ort.get_available_providers()
        self.ort_session = ort.InferenceSession(path, providers=provider)
        logging.info(self.ort_session.get_providers())
        self.binding = self.ort_session.io_binding()

    def infer(self, inputs: torch.Tensor):
        # return self.infer_iobinding(inputs)
        return self.infer_naive(inputs)

    def infer_naive(self, inputs: torch.Tensor):
        """inefficient in gpu-cpu transfer if inputs are on already gpu"""
        out = self.ort_session.run(None, {"input": inputs.to('cpu').numpy()})
        return out

    def infer_iobinding(self, inputs: torch.Tensor):
        """work in progress: run correctly in small data, 
        but diverges if batchsize x #batches goes beyond a threshold (e.g., 1500)

        api: https://onnxruntime.ai/docs/api/python/api_summary.html#data-on-device
        """
        self.inputs = inputs.contiguous()
        device = 'cuda' if inputs.device == torch.device('cuda') else 'cpu'
        self.binding.bind_input(
            name='input',
            device_type=device, device_id=0, element_type=np.float32,
            shape=tuple(inputs.shape), buffer_ptr=inputs.data_ptr(),
        )
        self.move_tensor = torch.empty((inputs.shape[0], 2187),
                                       dtype=torch.float32,
                                       device=device).contiguous()
        self.binding.bind_output(
            name='move',
            device_type=device, device_id=0, element_type=np.float32,
            shape=tuple(self.move_tensor.shape),
            buffer_ptr=self.move_tensor.data_ptr(),
        )
        # binding.bind_output('move')
        self.value_tensor = torch.empty((inputs.shape[0], 1),
                                        dtype=torch.float32,
                                        device=device).contiguous()
        self.binding.bind_output(
            name='value',
            device_type=device, device_id=0, element_type=np.float32,
            shape=tuple(self.value_tensor.shape),
            buffer_ptr=self.value_tensor.data_ptr(),
        )
        # binding.bind_output('value')
        self.aux_tensor = torch.empty((inputs.shape[0], 9*9*22),
                                      dtype=torch.float32,
                                      device=device).contiguous()
        self.binding.bind_output(
            name='aux',
            device_type=device, device_id=0, element_type=np.float32,
            shape=tuple(self.aux_tensor.shape),
            buffer_ptr=self.aux_tensor.data_ptr(),
        )
        # binding.bind_output('aux')
        self.ort_session.run_with_iobinding(self.binding)
        return (self.move_tensor.to('cpu').numpy(),
                self.value_tensor.to('cpu').numpy(),
                self.aux_tensor.to('cpu').numpy())
        # out = binding.copy_outputs_to_cpu()


class TorchTRTInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        import torch_tensorrt
        super().__init__(device)
        with torch_tensorrt.logging.info():
            self.trt_module = torch.jit.load(path)
        self.device = device

    def infer(self, inputs: torch.Tensor):
        tensor = inputs.half().to(self.device)
        outputs = self.trt_module(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


class TorchScriptInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        super().__init__(device)
        self.ts_module = torch.jit.load(path)

    def infer(self, inputs: torch.Tensor):
        with torch.no_grad():
            tensor = inputs.to(self.device)
            outputs = self.ts_module(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


class TorchInfer(InferenceModel):
    def __init__(self, model, device: str):
        super().__init__(device)
        self.model = model
        self.model.eval()
        self.device = device

    def infer(self, inputs: torch.Tensor):
        with torch.no_grad():
            tensor = inputs.to(self.device)
            outputs = self.model(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


def load(path: str, device: str = "", torch_cfg: dict = {},
         *,
         compiled: bool = False,
         remove_aux_head: bool = False) -> InferenceModel:
    """factory method to load a model from file

    :param path: filepath,
    :param device: torch device such as 'cuda', 'cpu',
    :param torch_cfg: network specification needed for TorchInfer.
    """
    if remove_aux_head:
        if not (path.endswith('.pt') or path.endswith('.chpt')):
            raise ValueError('not supported')
    if path.endswith('.onnx'):
        return OnnxInfer(path, device)
    if path.endswith('.ts'):
        if not device:
            device = 'cuda'
        return TorchTRTInfer(path, device)
    if path.endswith('.pts'):  # need to used a different extention from TRT's
        return TorchScriptInfer(path, device)
    if path.endswith('.pt'):
        NN = miniosl.network.PVNetwork if remove_aux_head else miniosl.network.StandardNetwork
        raw_model = NN(**torch_cfg).to(device)
        if compiled:
            cmodel = torch.compile(raw_model)
            model = cmodel
        else:
            model = raw_model
        saved_state = torch.load(path, map_location=torch.device(device))
        strict = not remove_aux_head
        model.load_state_dict(saved_state, strict=strict)
        return TorchInfer(raw_model, device)
    if path.endswith('.chpt'):
        checkpoint = torch.load(path, map_location=torch.device(device))
        cfg = checkpoint['cfg']
        network_cfg = cfg['network_cfg']
        for obsolete_key in ['make_bottleneck']:
            if obsolete_key in network_cfg:
                del network_cfg[obsolete_key]
        NN = miniosl.network.PVNetwork if remove_aux_head \
            else miniosl.network.StandardNetwork
        raw_model = NN(**network_cfg).to(device)
        if compiled:
            cmodel = torch.compile(raw_model)
            model = cmodel
        else:
            model = raw_model
        strict = not remove_aux_head
        model.load_state_dict(checkpoint['model_state_dict'], strict=strict)
        return TorchInfer(raw_model, device)
    raise ValueError("unknown filetype")


class InferenceForGameArray(miniosl.InferenceModelStub):
    def __init__(self, module: InferenceModel):
        super().__init__()
        self.module = module

    def py_infer(self, features):
        features = features.reshape(-1, len(miniosl.channel_id), 9, 9)
        return self.module.infer_int8(features)


def export_onnx(model, *, device, filename):
    import torch.onnx
    model.eval()
    dtype = torch.float
    dummy_input = torch.randn(1024, feature_channels, 9, 9, device=device,
                              dtype=dtype)
    if not filename.endswith('.onnx'):
        filename = f'{filename}.onnx'

    torch.onnx.export(model, dummy_input, filename,
                      dynamic_axes={'input': {0: 'batch_size'},
                                    'move': {0: 'batch_size'},
                                    'value': {0: 'batch_size'},
                                    'aux': {0: 'batch_size'}},
                      verbose=False, input_names=['input'],
                      output_names=['move', 'value', 'aux'])


def export_tensorrt(model, *, device, filename):
    logging.debug(f'feature challes {feature_channels}')
    import torch_tensorrt
    if not device:
        device = 'cuda'
    elif not device.startswith('cuda'):
        raise ValueError(f'unexpected device for trt {device}')
    model.eval()
    model = model.half()
    inputs = [
        torch_tensorrt.Input(
            min_shape=[1, feature_channels, 9, 9],
            opt_shape=[128, feature_channels, 9, 9],
            max_shape=[2048, feature_channels, 9, 9],
            dtype=torch.half,
        )]
    enabled_precisions = {torch.half}
    trt_ts_module = torch_tensorrt.compile(
        model, inputs=inputs, enabled_precisions=enabled_precisions,
        ir='ts',
        device=torch.device(device)
    )
    input_data = torch.randn(16, feature_channels, 9, 9, device=device)
    _ = trt_ts_module(input_data.half())
    savefile = filename if filename.endswith('.ts') else f'{filename}.ts'
    torch.jit.save(trt_ts_module, savefile)


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
        export_tensorrt(model, device=device, filename=filename)
    elif filename.endswith('.pts'):
        export_torch_script(model, device=device, filename=filename)
    else:
        raise ValueError("unknown filetype")
