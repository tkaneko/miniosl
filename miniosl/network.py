from __future__ import annotations
import torch
from torch import nn


class ResBlock(nn.Module):
    def __init__(self, block: nn.Module):
        super().__init__()
        self.block = block

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return self.block(data) + data


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


class BasicNetwork(nn.Module):
    def __init__(self, *, in_channels: int, channels: int, out_channels: int):
        super().__init__()
        self.conv1 = nn.Conv2d(in_channels, channels, 3, padding=1)
        self.block1 = ResBlock(
            nn.Sequential(
                nn.Conv2d(channels, channels, 3, padding=1),
                nn.ReLU(),
                nn.Conv2d(channels, channels, 3, padding=1),
                nn.ReLU(),
            )
        )
        self.block2 = ResBlock(
            nn.Sequential(
                nn.Conv2d(channels, channels, 3, padding=1),
                nn.ReLU(),
                nn.Conv2d(channels, channels, 3, padding=1),
                nn.ReLU(),
            )
        )
        self.head = PolicyHead(channels=channels, out_channels=out_channels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = nn.functional.relu(self.conv1(x))
        x = self.block1(x)
        x = self.block2(x)
        return self.head(x)

# Local Variables:
# python-indent-offset: 4
# End:
