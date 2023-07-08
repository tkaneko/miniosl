import miniosl
import torch
import numpy as np
import logging
import abc


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
    @abc.abstractmethod
    def infer(self, inputs: torch.Tensor):
        """return inference results for batch"""
        pass

    def infer_one(self, input: np.ndarray):
        outputs = self.infer(torch.stack((torch.from_numpy(input),)))
        return [_[0] for _ in outputs]

    def eval(self, input: np.ndarray):
        out = self.infer_one(input)
        moves, value, aux = (softmax(out[0]).reshape(-1, 9, 9),
                             out[1].item(), out[2].reshape(-1, 9, 9))
        return (moves, value, aux)


class OnnxInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        import onnxruntime as ort
        if device == 'cpu' or not device:
            provider = ['CPUExecutionProvider']
        else:
            provider = ort.get_available_providers(path, providers=provider)
            logging.info(self.ort_session.get_providers())
        self.ort_session = ort.InferenceSession(path)

    def infer(self, inputs):
        out = self.ort_session.run(None, {"input": inputs.numpy()})
        return out


class TorchTRTInfer(InferenceModel):
    def __init__(self, path: str, device: str):
        import torch_tensorrt
        with torch_tensorrt.logging.info():
            self.trt_module = torch.jit.load(path)
        self.device = device

    def infer(self, inputs):
        tensor = inputs.half().to(self.device)
        outputs = self.trt_module(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


class TorchInfer(InferenceModel):
    def __init__(self, path: str, device: str, network_cfg: dict):
        self.model = miniosl.network.StandardNetwork(**network_cfg).to(device)
        saved_state = torch.load(path, map_location=torch.device(device))
        self.model.load_state_dict(saved_state)
        self.model.eval()
        self.device = device

    def infer(self, inputs):
        with torch.no_grad():
            tensor = inputs.to(self.device)
            outputs = self.model(tensor)
        return [_.to('cpu').numpy() for _ in outputs]


def load(path: str, device: str = "", torch_cfg: dict = {}) -> InferenceModel:
    if path.endswith('.onnx'):
        return OnnxInfer(path, device)
    if path.endswith('.ts'):
        if not device:
            device = 'cuda'
        return TorchTRTInfer(path, device)
    if path.endswith('.pt'):
        return TorchInfer(path, device, torch_cfg)
    raise ValueError("unknown filetype")
