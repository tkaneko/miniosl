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
        return nn.functional.silu(self.block(data) + data)


class Conv2d(nn.Module):
    """a variant of `nn.Conv2d` separiting bias terms as BatchNorm2d.
    The parameters follow the pseudocode in the Gumbel MuZero's paper,
    except for BN; (1) scaling is fixed in the original but
    learnable here due to the lack of api in pytorch, and (2) momentum
    is relaxed than the original of 0.001 to speed up learning.
    """
    def __init__(self, in_channels: int, out_channels: int, kernel_size: int,
                 *, padding: int = 0, stride=1, groups=1):
        super().__init__()
        self.conv = nn.Conv2d(in_channels, out_channels, kernel_size,
                              padding=padding, bias=False,
                              stride=stride, groups=groups)
        self.bn = nn.BatchNorm2d(out_channels, eps=1e-3)

    def forward(self, data: torch.Tensor) -> torch.Tensor:
        return self.bn(self.conv(data))


class ConvTranspose2d(nn.Module):
    def __init__(self, in_channels: int, out_channels: int, kernel_size: int,
                 *, padding: int = 0, stride=1, groups=1):
        super().__init__()
        self.conv = nn.ConvTranspose2d(in_channels, out_channels, kernel_size,
                                       padding=padding, bias=False,
                                       stride=stride, groups=groups)
        self.bn = nn.BatchNorm2d(out_channels, eps=1e-3)

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
            nn.Linear(hidden_layer_size, 4),
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
        bmax = nn.functional.max_pool2d(b, kernel_size=9)
        bmean = nn.functional.avg_pool2d(b, kernel_size=9)
        b = torch.cat((bmax, bmean),
                      dim=1).squeeze(-1).squeeze(-1)
        b = self.linear(b)
        c = a + b[:, :, None, None]
        return self.conv1x1out(c)


class KatagoBlock(nn.Module):
    """Nested bottleneck residual architecture described in KataGo's doc
    with an additional broadcasting path.

    This block places four (instead of two) 3x3 cnn layers inside a
    bottleneck part for efficiency.  The additional path provides a
    global information by broadcasting at the end of the bottleneck
    skipping four 3x3 cnn layers.  A file oriented convolution gives a
    relatively good balance between the cost and accuracy.  Note that
    the kernel shape of 9x1 is not a popular one but already tried
    in shogi though in a bit different context.

    https://github.com/lightvector/KataGo/blob/master/docs/KataGoMethods.md#nested-bottleneck-residual-nets
    https://raw.githubusercontent.com/lightvector/KataGo/master/images/docs/bottlenecknestedresblock.png
    https://www.apply.computer-shogi.org/wcsc33/appeal/Ryfamate/appeal_ryfamate_20230421.pdf

    """
    def __init__(self, channels: int, bottleneck_scale=2):
        super().__init__()
        b_channels = channels // bottleneck_scale
        self.convin = Conv2d(channels, b_channels, 1)
        self.block1 = nn.Sequential(
            Conv2d(b_channels, b_channels, 3, padding=1),
            nn.ReLU(),
            Conv2d(b_channels, b_channels, 3, padding=1),
        )
        self.block2 = nn.Sequential(
            Conv2d(b_channels, b_channels, 3, padding=1),
            nn.ReLU(),
            Conv2d(b_channels, b_channels, 3, padding=1),
        )
        self.convout = Conv2d(b_channels, channels, 1)
        self.conv_filea = Conv2d(b_channels, b_channels, (9, 1))

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        a = nn.functional.relu(self.convin(x))
        b = self.block1(a)
        c = nn.functional.silu(a + b)
        d = self.block2(c)
        files = self.conv_filea(a)
        e = nn.functional.silu(c + files + d)
        f = self.convout(e)
        return nn.functional.silu(x + f)


def make_gumbel_az_block(channels: int) -> nn.Module:
    b_channels = channels // 2
    return ResBlockAlt(
        nn.Sequential(
            Conv2d(channels, b_channels, 1),
            nn.ReLU(),
            Conv2d(b_channels, b_channels, 3, padding=1),
            nn.ReLU(),
            Conv2d(b_channels, b_channels, 3, padding=1),
            nn.ReLU(),
            Conv2d(b_channels, channels, 1),
        ))


class BasicBody(nn.Module):
    """Body of networks to provide a good feature vector for heads.

    Typical composition would be
    - [KatagoBlock(256, 4) x 2 + PoolBias] x 3, or
    - [gumbel_az_block(128) x 3 + PoolBias] x 3.
    """
    def __init__(self, *, in_channels: int, channels: int, num_blocks: int,
                 broadcast_every: int = 8):
        super().__init__()
        self.conv1 = Conv2d(in_channels, channels, 3, padding=1)
        self.body = nn.Sequential(
            *[KatagoBlock(channels, 4)  # make_gumbel_az_block(channels)
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
                 value_head_hidden: int = 256, broadcast_every: int = 3):
        super().__init__()
        self.body = BasicBody(in_channels=in_channels, channels=channels,
                              num_blocks=num_blocks,
                              broadcast_every=broadcast_every)
        self.head = PolicyHead(channels=channels, out_channels=out_channels)
        self.value_head = ValueHead(channels=channels,
                                    hidden_layer_size=value_head_hidden)
        self.config = {
            'in_channels': in_channels, 'channels': channels,
            'out_channels': out_channels,  # no aux
            'num_blocks': num_blocks, 'value_head_hidden': value_head_hidden,
            'broadcast_every': broadcast_every,
        }

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        """take a batch of input features
        and return a batch of [policies, values].
        """
        x = self.body(x)
        return self.head(x), self.value_head(x)

    def save_with_dict(self, filename):
        """save config and weights into .ptd file"""
        torch.save({'cfg': self.config,
                    'model_state_dict': self.state_dict()},
                   filename)

    @classmethod
    def load_with_dict(cls, filename):
        """make a module with configs and weights saved in .ptd file"""
        import logging
        import json
        objs = torch.load(filename, map_location=torch.device('cpu'))
        cfg = objs['cfg']
        model = cls(**cfg)
        logging.debug(json.dumps(cfg, indent=4))
        if 'model_state_dict' in objs:
            model.load_state_dict(objs['model_state_dict'])
        return model

    def clone(self):
        """clone a module with current weights"""
        cls = self.__class__
        obj = cls(**self.config)
        obj.load_state_dict(self.state_dict())
        return obj

    def soft_update(self, new_state_dict, tau: float = .5,
                    keys = None):
        """update weights with new ones"""
        my_parameters = self.state_dict()
        if not keys:
            keys = my_parameters.keys()
        for key in keys:
            old_value = my_parameters[key]
            new_value = new_state_dict[key]
            my_parameters[key] = tau * new_value + (1-tau) * old_value
        self.load_state_dict(my_parameters)


class StandardNetwork(PVNetwork):
    """Standard residual networks with bottleneck architecture

    :param in_channels: number of channels in input features,
    :param channels: number of channels in main body,
    :param out_channels: number of channels in policy_head,
    :param auxout_channels: number of channels in miscellaneous output,
    :param value_head_hidden: hidden units in the last layer in the value head
    """
    def __init__(self, *, in_channels: int, channels: int, out_channels: int,
                 auxout_channels: int, num_blocks: int,
                 value_head_hidden: int = 256, broadcast_every: int = 3):
        super().__init__(in_channels=in_channels, channels=channels,
                         out_channels=out_channels,
                         num_blocks=num_blocks,
                         value_head_hidden=value_head_hidden,
                         broadcast_every=broadcast_every)
        self.aux_head = PolicyHead(channels=channels,
                                   out_channels=auxout_channels)
        self.config['auxout_channels'] = auxout_channels

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
