"""player"""

import miniosl
import logging
import json
import re


class MyRandomPlayer(miniosl.SingleCPUPlayer):
    """sample of implementing virtual methods in python
    """
    def __init__(self):
        super().__init__()

    def think(self, usi: str):
        record = miniosl.usi_record(usi)
        state = record.replay(-1)
        moves = state.genmove()
        move = moves[0]
        return move

    def name(self):
        return "py-random"


class UsiPlayer(miniosl.SingleCPUPlayer):
    """bridge between usi engine and :py:class:`CPUPlayer`
    for self-play with :py:class:`GameArray`.
    """
    def __init__(self, engine: miniosl.UsiProcess):
        super().__init__()
        self.engine = engine

    def think(self, usi: str):
        record = miniosl.usi_record(usi)
        state = record.replay(-1)
        go = 'byoyomi 1000'
        ret = self.engine.search(usi, go)
        move = ret['move']
        return state.to_move(move)

    def name(self):
        id = self.engine.name
        return id.replace(' ', '_') if id else "usi"


def make_usi_player(cfg_path: str) -> UsiPlayer:
    with open(cfg_path, 'r') as f:
        cfg = json.load(f)
    if 'type' not in cfg or cfg['type'] != 'usi':
        raise ValueError(f'unsupported cfg {cfg_path}')
    path_and_args = cfg['path_and_args']
    setoptions = cfg['setoptions']
    cwd = cfg['cwd'] if 'cwd' in cfg else None
    engine = miniosl.UsiProcess(path_and_args, setoptions, cwd)
    if not hasattr(make_usi_player, 'players'):
        make_usi_player.players = []
    pl = UsiPlayer(engine)
    make_usi_player.players.append(pl)
    return miniosl.CPUPlayer(pl, False)


def clear_usi_players():
    if hasattr(make_usi_player, 'players'):
        make_usi_player.players = []


def make_player(
        name: str, *,
        noise_scale=1.0, softalpha=0.00783,
        depth_weight=0.5, book_path='') -> miniosl.PlayerArray:
    """factory method"""
    gumbel_pattern = r'^gumbel([1-9][0-9]*)$'
    gumbel_d2_pattern = r'^gumbel([1-9][0-9]*)-([1-9][0-9]*)$'
    soft_pattern = r'^soft([1-9][0-9]*)$'
    soft_d2_pattern = r'^soft([1-9][0-9]*)-([1-9][0-9]*)$'
    td_pattern = r'^gumbeltd([1-9][0-9]*)$'
    ave_pattern = r'^gumbelave([1-9][0-9]*)$'
    max_pattern = r'^gumbelmax([1-9][0-9]*)$'
    config = miniosl.GumbelPlayerConfig()
    config.noise_scale = noise_scale
    config.depth_weight = depth_weight
    if book_path:
        config.book_path = book_path
    config.book_weight_p = 0    # todo
    if match := re.match(gumbel_d2_pattern, name):
        config.root_width = int(match.group(1))
        config.second_width = int(match.group(2))
        if config.root_width < config.second_width:
            raise RuntimeError(
                f'width mismatch {config.root_width} < {config.second_width}'
            )
        return miniosl.FlatGumbelPlayer(config)
    elif match := re.match(gumbel_pattern, name):
        config.root_width = int(match.group(1))
        return miniosl.FlatGumbelPlayer(config)
    elif match := re.match(td_pattern, name):
        config.root_width = int(match.group(1))
        config.value_mix = 1
        return miniosl.FlatGumbelPlayer(config)
    elif match := re.match(ave_pattern, name):
        config.root_width = int(match.group(1))
        config.value_mix = 2
        return miniosl.FlatGumbelPlayer(config)
    elif match := re.match(max_pattern, name):
        config.root_width = int(match.group(1))
        config.value_mix = 3
        return miniosl.FlatGumbelPlayer(config)
    elif match := re.match(soft_d2_pattern, name):
        config.root_width = int(match.group(1))
        config.second_width = int(match.group(2))
        if config.root_width < config.second_width:
            raise RuntimeError(
                f'width mismatch {config.root_width} < {config.second_width}'
            )
        config.softalpha = softalpha
        return miniosl.FlatGumbelPlayer(config)
    elif match := re.match(soft_pattern, name):
        config.root_width = int(match.group(1))
        config.softalpha = softalpha
        return miniosl.FlatGumbelPlayer(config)
    elif name == "greedy-policy":
        return miniosl.PolicyPlayer(True)
    elif name == "random":
        """ ... warning:: this depends on std::shared_ptr is used as holder
        for :py:class:`RandomPlayer`, otherwise results in dangling pointer.
        """
        return miniosl.CPUPlayer(miniosl.RandomPlayer(), False)
    elif name == "myrandom":
        """save before return to keep its lifetime
        """
        make_player.random = MyRandomPlayer()
        return miniosl.CPUPlayer(make_player.random, False)
    elif name.endswith('.json'):
        return make_usi_player(name)
    else:
        if name != "policy":
            logging.warn(f"unknown player name {name}")
        return miniosl.PolicyPlayer()
