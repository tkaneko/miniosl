"""mcts and others"""
import miniosl
import numpy as np
import math
import copy
import collections

#  -------------------------------------------------
#  az configs see pseudocode in the AZ Science paper


az_pb_c_base = 19652
az_pb_c_init = 1.25


def az_ucb_score(parent, child):
    if child.is_terminal():
        return 0.
    pb_c = math.log((parent.visit_count + az_pb_c_base + 1) /
                    az_pb_c_base) + az_pb_c_init
    count = child.visit_count + child.unobserved
    pb_c *= math.sqrt(parent.visit_count) / (count + 1)
    prior_score = pb_c * child.policy_probability
    assert prior_score > 0
    value_score = 1 - child.value()  # view f rom parent

    return prior_score + value_score
#  -------------------------------------------------


def az_value_transform(v: float | np.ndarray) -> float:
    """transform value in range [-1,1] to [0,1]"""
    return v/2 + 0.5


TreeStat = collections.namedtuple('TreeStat',
                                  ('max_depth', 'nodes', 'unobserved'))


class Node:
    """MCTS node"""
    def __init__(self, parent, policy_probability):
        assert policy_probability > 0
        self.visit_count = 0
        self.unobserved = 0     # Watch the Unobserved (2020)
        self.value_subtree_sum = 0.
        self.children = {}
        self.loss_children = {}
        self.parent = parent
        self.policy_probability = policy_probability
        self.terminal_value = None
        self.in_check = None

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

    def loss_ratio(self) -> bool:
        eps = 1e-8
        loss_total = len(self.loss_children)
        moves_tried = sum([min(1, c.visit_count)
                           for c in self.children.values()])
        return loss_total / (loss_total + moves_tried + eps)

    def in_threatmate(self) -> bool:
        return (not self.in_check) and (len(self.loss_children) > 2
                                        or self.loss_ratio() > 0.3)

    def eval0(self, game: miniosl.GameManager):
        self.in_check = game.state.in_check()
        if game.state.in_checkmate() or len(game.legal_moves) == 0:
            self.terminal_value = 0.
        else:
            move = game.state.try_checkmate_1ply()
            if move.is_normal() or game.state.win_if_declare():
                self.terminal_value = 1.
                self.winning_move = move \
                    if move.is_normal() else move.declare_win()

    def eval1(self, legal_moves, mp, value01) -> float:
        for prob, move in zip(mp, legal_moves):
            assert prob > 0
            self.children[move] = Node(self, prob)
        self.top5 = sorted(zip(legal_moves, mp), key=lambda e: -e[1])
        self.top5 = self.top5[:min(5, len(mp))]
        return value01

    def eval_by_model(self,
                      features, legal_moves,
                      model: miniosl.InferenceModel):
        logits, value, _ = model.infer_one(features)
        value01 = az_value_transform(value.item())
        mp = np.array([logits[_.policy_move_label()]
                       for _ in legal_moves])
        mp = miniosl.softmax(mp).tolist()
        return mp, value01

    def eval(self, game: miniosl.GameManager, model: miniosl.InferenceModel
             ) -> float:
        self.eval0(game)
        if self.is_terminal():
            return self.terminal_value
        mp, value01 = self.eval_by_model(game.export_heuristic_feature16(),
                                         game.legal_moves,
                                         model)
        return self.eval1(game.legal_moves, mp, value01)

    def select_to_search(self):
        ucb_scores = [(az_ucb_score(self, child), move, child)
                      for move, child in self.children.items()]
        _, move, child = max(ucb_scores)
        self.top5_ucb = sorted(ucb_scores, key=lambda e: -e[0])
        self.top5_ucb = self.top5_ucb[:len(self.top5)]
        return move, child

    def info(self):
        """summary of node statistics"""
        return f'val={self.value():.3f} vis={self.visit_count}' \
            + f' t={self.terminal_value}' \
            + ' pol=' \
            + ','.join([f'({_[0].to_csa()},{_[1]*100:.1f}%)'
                        for _ in self.top5]) \
            + ' ucb=' \
            + ','.join([f'{_[1].to_csa()},{_[0]:.2f},'
                        f'{_[2].policy_probability*100:.1f}'
                        for _ in self.top5_ucb])

    def reasonable(self, child, threshold):
        """check child's terminal value is consistent with win at self
        (compare visit_counts with threshold otherwise)
        """
        if not self.is_terminal() or self.terminal_value == 0:
            return self.visit_count >= threshold
        return child.is_terminal() \
            and self.terminal_value + child.terminal_value == 1

    def visited_children(self, threshold: int = 1):
        return [_ for _ in
                sorted(self.children.items() or self.loss_children.items(),
                       key=lambda e: -e[1].visit_count)
                if self.reasonable(_[1], threshold)
                ]

    def pv(self, ratio=0.25, prefix=[], sign=-1):
        children = self.visited_children(int(self.visit_count*ratio))
        if not children:
            return prefix
        move, c = children[0]
        v = sign*c.value() + (1-sign)//2  # v or 1-v
        return c.pv(ratio,
                    prefix+[(move, v, c.visit_count, c.in_threatmate())],
                    -sign)

    def tree_stat(self):
        max_depth = 0
        node_count = 1
        unobserved_total = 0
        if self.visit_count > 0:
            for move, child in self.children.items():
                depth, nodes, unobserved = child.tree_stat()
                max_depth = max(depth + 1, max_depth)
                unobserved_total += unobserved
                if child.visit_count > 0:
                    node_count += nodes
        return TreeStat(max_depth, node_count, unobserved_total)


Record = collections.namedtuple('Record',
                                ('path', 'node', 'features', 'legal_moves'))


def process_batch_seq(batch, model):
    for r in batch:
        mp, value01 = r.node.eval_by_model(r.features, r.legal_moves, model)
        r.node.eval1(r.legal_moves, mp, value01)
        update_path(r.path, value01, r.node)


def process_batch(batch, model):
    batch_input = np.vstack([[r.features] for r in batch])
    mps, values, _ = model.infer_int8(batch_input)
    value01s = az_value_transform(values)
    for i, record in enumerate(batch):
        mp = miniosl.softmax(mps[i]).tolist()
        value01 = value01s[i].item()
        record.node.eval1(record.legal_moves, mp, value01)
        update_path(record.path, value01, record.node)


def update_path(path, value, leaf):
    node, child = leaf, None
    last_move = None
    while node is not None:
        if child and child.is_terminal():
            if value == 1:
                node.terminal_value = 1
                if last_move:
                    assert node.children[last_move] == child
                    node.winning_move = last_move
            elif value == 0:
                assert node.children[last_move] == child
                node.loss_children[last_move] = child
                del node.children[last_move]
                if len(node.children) == 0:
                    node.terminal_value = 0

        node.value_subtree_sum += value
        node.visit_count += 1
        node.unobserved -= 1

        node, child = node.parent, node
        value = 1 - value
        last_move = path.pop() if node else None


def run_mcts(game: miniosl.GameManager, budget: int,
             model: miniosl.InferenceModel,
             *, batch_size=1, root=None) -> Node:
    """plain mcts, no batching"""
    if root is None:
        root = Node(None, 1.0)
        value = root.eval(game, model)
        update_path([], value, root)

    batch = []
    for i in range(budget):
        if root.is_terminal():
            break

        node = root
        search = copy.copy(game)
        path = []
        while node.has_children():
            move, child = node.select_to_search()
            search.make_move(move)
            path.append(move)
            node = child
            node.unobserved += 1

        if batch_size == 1:
            value = node.eval(search, model)
            update_path(path, value, node)
            continue

        # batch_size > 1
        node.eval0(search)
        if node.is_terminal():
            value = node.terminal_value
            update_path(path, value, node)
            continue

        features = search.export_heuristic_feature8()
        batch.append(Record(copy.copy(path), node, features,
                            search.legal_moves))

        if len(batch) >= batch_size:
            process_batch(batch, model)
            batch = []
    if len(batch) >= batch_size:
        # if any remains
        process_batch(batch, model)
    return root
