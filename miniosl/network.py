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
        return nn.functional.relu(self.block(data) + data)


class Conv2d(nn.Module):
    """a variant of `nn.Conv2d` separiting bias terms as BatchNorm2d.
    The parameters follow the pseudocode in the Gumbel MuZero's paper,
    except for BN; (1) scaling is fixed in the original but
    learnable here due to the lack of api in pytorch, and (2) momentum
    is relaxed than the original of 0.001 to speed up learning.
    """
    def __init__(self, in_channels: int, out_channels: int, kernel_size: int,
                 *, padding: int = 0, bn_momentum: float = 0.1):
        super().__init__()
        self.conv = nn.Conv2d(in_channels, out_channels, kernel_size,
                              padding=padding, bias=False)
        self.bn = nn.BatchNorm2d(out_channels,
                                 momentum=bn_momentum,
                                 eps=1e-3)

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return self.bn(self.conv(data))


class PolicyHead(nn.Module):
    def __init__(self, *, channels: int, out_channels: int):
        super().__init__()
        self.head = nn.Sequential(
            Conv2d(channels, channels, 1),
            nn.ReLU(),
            Conv2d(channels, out_channels, 1),
            nn.Flatten()
        )

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return self.head(data)


class ValueHead(nn.Module):
    def __init__(self, channels: int, hidden_layer_size):
        super().__init__()
        self.head = nn.Sequential(
            Conv2d(channels, 1, 1),
            nn.ReLU(),
            nn.Flatten(),
            nn.Linear(81, hidden_layer_size),
            nn.ReLU(),
            nn.Linear(hidden_layer_size, 1),
            nn.Tanh(),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.head(x)


class PoolBias(nn.Module):
    """an interpretation of Fig. 12 in the Gumbel MuZero paper"""
    def __init__(self, *, channels: int):
        super().__init__()
        self.channels = channels
        self.conv1x1a = Conv2d(channels, channels, 1)
        self.conv1x1b = Conv2d(channels, channels, 1)
        self.conv1x1out = Conv2d(channels, channels, 1)
        self.linear = nn.Linear(2*channels, channels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        a = nn.functional.relu(self.conv1x1a(x))
        b = nn.functional.relu(self.conv1x1b(x))
        b = b.reshape(-1, self.channels, 81)
        bmax = torch.max(b, dim=2)
        bmean = torch.mean(b, dim=2)
        b = torch.cat((bmax[0], bmean), dim=1)
        b = self.linear(b)
        c = a + b[:, :, None, None]
        return self.conv1x1out(c)


def make_block(channels: int, bn_momentum: int) -> nn.Module:
    return ResBlockAlt(
        nn.Sequential(
            Conv2d(channels, channels//2, 1,
                   bn_momentum=bn_momentum),
            nn.ReLU(),
            Conv2d(channels//2, channels//2, 3, padding=1,
                   bn_momentum=bn_momentum),
            nn.ReLU(),
            Conv2d(channels//2, channels//2, 3, padding=1,
                   bn_momentum=bn_momentum),
            nn.ReLU(),
            Conv2d(channels//2, channels, 1,
                   bn_momentum=bn_momentum),
        ))


class BasicBody(nn.Module):
    def __init__(self, *, in_channels: int, channels: int, num_blocks: int,
                 broadcast_every: int = 8,
                 bn_momentum: float = 0.1):
        super().__init__()
        self.conv1 = Conv2d(in_channels, channels, 3, padding=1)
        self.body = nn.Sequential(
            *[make_block(channels, bn_momentum)
              if (_ + 1) % broadcast_every != 0
              else ResBlockAlt(PoolBias(channels=channels))
              for _ in range(num_blocks)
              ])

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = nn.functional.relu(self.conv1(x))
        x = self.body(x)
        return x


class BasicNetwork(nn.Module):
    def __init__(self, *, in_channels: int, channels: int, out_channels: int):
        super().__init__()
        self.body = BasicBody(in_channels=in_channels, channels=channels,
                              num_blocks=2)
        self.head = PolicyHead(channels=channels, out_channels=out_channels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.body(x)
        return self.head(x)


class PVNetwork(nn.Module):
    def __init__(self, *, in_channels: int, channels: int, out_channels: int,
                 num_blocks: int,
                 value_head_hidden: int = 256, broadcast_every: int = 8,
                 bn_momentum: float = 0.1,
                 auxout_channels=None):
        super().__init__()
        self.body = BasicBody(in_channels=in_channels, channels=channels,
                              num_blocks=num_blocks,
                              broadcast_every=broadcast_every,
                              bn_momentum=bn_momentum)
        self.head = PolicyHead(channels=channels, out_channels=out_channels)
        self.value_head = ValueHead(channels=channels,
                                    hidden_layer_size=value_head_hidden)

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        """take a batch of input features
        and return a batch of [policies, values, misc].
        """
        x = self.body(x)
        return self.head(x), self.value_head(x)


class StandardNetwork(PVNetwork):
    """Standard residual networks with bottleneck architecture in torch

    :param in_channels: number of channels in input features,
    :param channels: number of channels in main body,
    :param out_channels: number of channels in policy_head,
    :param auxout_channels: number of channels in miscellaneous output,
    :param value_head_hidden: hidden units in the last layer in the value head
    """
    def __init__(self, *, in_channels: int, channels: int, out_channels: int,
                 auxout_channels: int, num_blocks: int,
                 value_head_hidden: int = 256, broadcast_every: int = 8,
                 bn_momentum: float = 0.1):
        super().__init__(in_channels=in_channels, channels=channels, out_channels=out_channels,
                         num_blocks=num_blocks,
                         value_head_hidden=value_head_hidden, broadcast_every=broadcast_every,
                         bn_momentum=bn_momentum)
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
