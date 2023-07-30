"""mcts and others"""
import miniosl
import numpy as np
import math
import copy
import logging

#  -------------------------------------------------
#  az configs see pseudocode in the AZ Science paper


az_pb_c_base = 19652
az_pb_c_init = 1.25


def az_ucb_score(parent, child):
    if child.is_terminal():
        return 0.
    pb_c = math.log((parent.visit_count + az_pb_c_base + 1) /
                    az_pb_c_base) + az_pb_c_init
    pb_c *= math.sqrt(parent.visit_count) / (child.visit_count + 1)
    prior_score = pb_c * child.policy_probability
    assert prior_score > 0
    value_score = 1 - child.value()  # view f rom parent

    return prior_score + value_score
#  -------------------------------------------------


def az_value_transform(v: float) -> float:
    """transform value in range [-1,1] to [0,1]"""
    return v/2 + 0.5


class Node:
    """MCTS node"""
    def __init__(self, parent, policy_probability):
        self.visit_count = 0
        self.value_subtree_sum = 0.
        self.children = {}
        self.loss_children = {}
        self.parent = parent
        self.policy_probability = policy_probability
        self.terminal_value = None

    def value(self):
        if self.terminal_value is not None:
            return self.terminal_value
        if self.visit_count == 0:
            return 0.
        return self.value_subtree_sum / self.visit_count

    def has_children(self) -> bool:
        return len(self.children) + len(self.loss_children) > 0

    def is_terminal(self) -> bool:
        return self.terminal_value is not None

    def eval(self, game: miniosl.GameManager, model: miniosl.InferenceModel):
        if game.state.in_checkmate() or game.state.in_no_legalmoves():
            self.terminal_value = 0.
        else:
            move = game.state.try_checkmate_1ply()
            if move.is_normal() or game.state.win_if_declare():
                self.terminal_value = 1.
        if self.is_terminal():
            logging.info(f'terminal {game.state}, {self.terminal_value}')
            return self.terminal_value
        logits, value, _ = model.infer_one(game.export_heuristic_feature())
        value01 = az_value_transform(value.item())
        mp = np.array([logits[_.policy_move_label()] for _ in game.legal_moves])
        mp = miniosl.softmax(mp).tolist()
        for prob, move in zip(mp, game.legal_moves):
            self.children[move] = Node(self, prob)
        self.top5 = sorted(zip(game.legal_moves, mp), key=lambda e: -e[1])
        self.top5 = self.top5[:min(5, len(mp))]
        return value01

    def select_to_search(self):
        ucb_scores = [(az_ucb_score(self, child), move, child)
                      for move, child in self.children.items()]
        _, move, child = max(ucb_scores)
        self.top5_ucb = sorted(ucb_scores, key=lambda e: -e[0])
        self.top5_ucb = self.top5_ucb[:len(self.top5)]
        return move, child

    def info(self):
        return f'val={self.value()} vis={self.visit_count}' \
          + f' t={self.terminal_value}' \
          + ' pol=' \
          + ','.join([f'({_[0].to_csa()},{_[1]*100:.1f}%)' for _ in self.top5]) \
          + ' ucb=' \
          + ','.join([f'{_[1].to_csa()},{_[0]:.2f},{_[2].policy_probability*100:.1f}' for _ in self.top5_ucb])


def update_path(path, value, leaf):
    node, child = leaf, None
    while node is not None:
        if child and child.is_terminal():
            if value == 1:
                node.terminal_value = 1
            elif value == 0:
                move = path.pop()
                assert node.children[move] == child
                node.loss_children[move] = child
                del node.children[move]
                if len(node.children) == 0:
                    node.terminal_value = 0

        node.value_subtree_sum += value
        node.visit_count += 1

        node, child = node.parent, node
        value = 1 - value


def run_mcts(game: miniosl.GameManager, budget: int,
             model: miniosl.InferenceModel) -> Node:
    root = Node(None, 0.0)
    value = root.eval(game, model)
    update_path([], value, root)

    for i in range(budget):
        if root.is_terminal():
            break

        node = root
        search = copy.copy(game)
        path = []
        while node.has_children():
            move, child = node.select_to_search()
            # if node == root:
            #     logging.info(f'new rollout {i} {move.to_csa()} {root.info()}')
            search.make_move(move)
            path.append(move)
            node = child

        value = node.eval(search, model)
        update_path(path, value, node)
    return root
