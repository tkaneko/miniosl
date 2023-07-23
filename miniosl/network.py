"""neural networks in pytorch"""
from __future__ import annotations
import torch
from torch import nn
from typing import Tuple


class ResBlock(nn.Module):
    def __init__(self, block: nn.Module):
        super().__init__()
        self.block = block

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return self.block(data) + data


class ResBlockAlt(nn.Module):
    def __init__(self, block: nn.Module):
        super().__init__()
        self.block = block

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return nn.ReLu(self.block(data) + data)


class PolicyHead(nn.Module):
    def __init__(self, *, channels: int, out_channels: int):
        super().__init__()
        self.head = nn.Sequential(
          nn.Conv2d(channels, channels, 1),
          nn.ReLU(),
          nn.Conv2d(channels, out_channels, 1),
          nn.Flatten()
        )

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return self.head(data)


class ValueHead(nn.Module):
    def __init__(self, channels: int, hidden_layer_size):
        super().__init__()
        self.head = nn.Sequential(
            nn.Conv2d(channels, 1, 1),
            nn.ReLU(),
            nn.Flatten(),
            nn.Linear(81, hidden_layer_size),
            nn.ReLU(),
            nn.Linear(hidden_layer_size, 1),
            nn.Tanh(),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.head(x)


class BasicBody(nn.Module):
    def __init__(self, *, in_channels: int, channels: int, num_blocks: int = 2,
                 make_bottleneck: bool = False):
        super().__init__()
        self.conv1 = nn.Conv2d(in_channels, channels, 3, padding=1)
        if make_bottleneck:
            self.body = nn.Sequential(
                *[ResBlockAlt(
                    nn.Sequential(
                        nn.Conv2d(channels, channels//2, 1),
                        nn.ReLU(),
                        nn.Conv2d(channels//2, channels//2, 3, padding=1),
                        nn.ReLU(),
                        nn.Conv2d(channels//2, channels//2, 3, padding=1),
                        nn.ReLU(),
                        nn.Conv2d(channels//2, channels, 1),
                    )) for _ in range(num_blocks)])
        else:
            self.body = nn.Sequential(
                *[ResBlock(
                    nn.Sequential(
                        nn.Conv2d(channels, channels, 3, padding=1),
                        nn.ReLU(),
                        nn.Conv2d(channels, channels, 3, padding=1),
                        nn.ReLU(),
                    )) for _ in range(num_blocks)])

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = nn.functional.relu(self.conv1(x))
        x = self.body(x)
        return x


class BasicNetwork(nn.Module):
    def __init__(self, *, in_channels: int, channels: int, out_channels: int):
        super().__init__()
        self.body = BasicBody(in_channels=in_channels, channels=channels)
        self.head = PolicyHead(channels=channels, out_channels=out_channels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.body(x)
        return self.head(x)


class StandardNetwork(nn.Module):
    """Standard residual networks with bottleneck architecture in torch

    :param in_channels: number of channels in input features,
    :param channels: number of channels in main body,
    :param out_channels: number of channels in policy_head,
    :param auxout_channels: number of channels in miscellaneous output,
    :param value_head_hidden: hidden units in the last layer in the value head
    """
    def __init__(self, *, in_channels: int, channels: int, out_channels: int,
                 auxout_channels: int, num_blocks: int, make_bottleneck: bool,
                 value_head_hidden: int = 256):
        super().__init__()
        self.body = BasicBody(in_channels=in_channels, channels=channels,
                              num_blocks=num_blocks)
        self.head = PolicyHead(channels=channels, out_channels=out_channels)
        self.value_head = ValueHead(channels=channels,
                                    hidden_layer_size=value_head_hidden)
        self.aux_head = PolicyHead(channels=channels,
                                   out_channels=auxout_channels)

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor,
                                                torch.Tensor]:
        """take a batch of input features
        and return a batch of [policies, values, misc].
        """
        x = self.body(x)
        return self.head(x), self.value_head(x), self.aux_head(x)


# Local Variables:
# python-indent-offset: 4
# End:
