"""player"""

import miniosl
import logging
import json


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


def make_player(name: str) -> miniosl.PlayerArray:
    """factory method"""
    if name == "gumbel8":
        return miniosl.FlatGumbelPlayer(8)
    elif name == "gumbel4":
        return miniosl.FlatGumbelPlayer(4)
    elif name == "greedy-policy":
        return miniosl.PolicyPlayer(True)
    elif name == "random":
        """ ... warning:: this depends on std::shared_ptr is used as holder
        for :py:class:`RandomPlayer`, otherwise results in dangling pointer.
        """
        return miniosl.CPUPlayer(miniosl.RandomPlayer(), False)
    elif name == "myrandom":
        """save befor return to keep its lifetime
        """
        make_player.random = MyRandomPlayer()
        return miniosl.CPUPlayer(make_player.random, False)
    elif name.endswith('.json'):
        return make_usi_player(name)
    else:
        if name != "policy":
            logging.warn(f"unknown player name {name}")
        return miniosl.PolicyPlayer()
