"""dataset for training in pytorch"""
import miniosl
import torch
import numpy as np
import logging
import os
import os.path
import recordclass


def load_sfen_from_file(sfen, *,
                        compress_and_rm: bool = False, strict: bool = False
                        ) -> list[miniosl.SubRecord]:
    """load game records from file for `GameRecordBlock`

    games must be completed to serve as training data.
    """
    sfen_npz = ''
    if sfen.endswith('.npz'):
        sfen_npz = sfen
    elif os.path.isfile(f'{sfen}.npz'):
        sfen_npz = f'{sfen}.npz'
    if os.path.isfile(sfen_npz):
        with open(sfen_npz, 'r') as f:
            record_set = miniosl.RecordSet.from_npz(sfen_npz)
            return [miniosl.SubRecord(_) for _ in record_set.records]
    data = miniosl.MiniRecordVector()
    ignored = 0
    with open(f'{sfen}', 'r') as f:
        for line in f:
            line = line.strip()
            record = miniosl.usi_record(line)
            if record.result == miniosl.InGame:
                if strict:
                    logging.warning(f'in game {line}')
                    raise ValueError('in game')
                ignored += 1
                continue
            data.append(record)
        if compress_and_rm:
            record_set = miniosl.RecordSet(data)
            record_set.save_npz(f'{sfen}.npz')
            os.remove(f'{sfen}')
    return [miniosl.SubRecord(_) for _ in data]


class GameDataset(torch.utils.data.Dataset):
    """dataset for training.

    - sample position by game index \
      (a random position in the game record is returned)
    - keep the latest `GameRecordBlock` s (e.g., 50) as in MuZero

    :param window_size: number of game records to keep
    :param block_unit: number of game records for a block, \
    that is a unit in add/replace oporation
    :param batch_with_collate: need to specify `collate_fn=lambda indices: dataset.collate(indices)` for trainloader, if (and only if) True
    """
    def __init__(self, window_size: int, block_unit: int,
                 batch_with_collate: bool = True,
                 opening_decay_power=None):
        self.window_size = window_size
        self.block_unit = block_unit
        self.blocks = miniosl.GameBlockVector()
        self.cur_block_id = 0
        self.block_limit = (window_size + block_unit - 1) // block_unit
        self.batch_with_collate = batch_with_collate

        self.blocks.reserve(self.block_limit)
        self.opening_decay_power = opening_decay_power

    def block_id(self) -> int:
        """number of block added so far"""
        return self.cur_block_id

    def unit_size(self) -> int:
        """number of records in a `GameRecordBlock`"""
        return self.block_unit

    def make_block(sfen, compress_and_rm=True, strict=True):
        """a helper function to make a block"""
        if isinstance(sfen, str):
            lst = load_sfen_from_file(sfen,
                                      compress_and_rm=compress_and_rm,
                                      strict=strict)
            return miniosl.GameRecordBlock(lst)
        elif isinstance(sfen, list):
            return miniosl.GameRecordBlock(sfen)
        raise ValueError(f'not supported {type(sfen)}')

    def add(self, new_block):
        """add or replace the oldest one with `new_block`"""
        if len(new_block) < self.unit_size():
            raise ValueError(f'size error {len(new_block)} {self.block_unit}')

        if len(self.blocks) < self.block_limit:
            self.blocks.append(new_block)
        else:
            self.blocks[self.cur_block_id % self.block_limit] = new_block
        self.cur_block_id += 1

    def stored_game_records(self):
        return self.block_unit * len(self.blocks)

    def sample_id(self, index):
        if not (0 <= index < len(self)):
            raise ValueError("index")
        # need this due to the spec of __len__(), even after fully filled
        index %= self.stored_game_records()
        return index // self.block_unit, index % self.block_unit

    def __len__(self):
        """epoch should go beyond #records to cover #positions"""
        return self.stored_game_records() * 100

    def __getitem__(self, idx):
        p, s = self.sample_id(idx)
        return ((p, s)
                if self.batch_with_collate else
                self.reshape_item(self.blocks[p].sample(s)))

    def reshape_item(self, item):
        input, move_label, value_label, aux_label = item
        return (torch.from_numpy(input), move_label, np.float32(value_label),
                torch.from_numpy(aux_label))

    def collate(self, indices):
        N = len(indices)
        inputs = np.zeros(N*miniosl.input_unit, dtype=np.int8)
        inputs2 = np.zeros(N*miniosl.input_unit, dtype=np.int8)
        policy_labels = np.zeros(N, dtype=np.int32)
        value_labels = np.zeros(N, dtype=np.float32)
        aux_labels = np.zeros(N*miniosl.aux_unit, dtype=np.int8)
        legalmove_labels = np.zeros(N * miniosl.legalmove_bs_sz,
                                    dtype=np.uint8)
        # sampled_ids = np.zeros(N, dtype=np.uint16)
        miniosl.collate_features(self.blocks, indices,
                                 inputs,
                                 policy_labels, value_labels, aux_labels,
                                 inputs2,
                                 legalmove_labels,
                                 # sampled_ids
                                 decay_power=self.opening_decay_power,
                                 )
        # for offset, (p, s) in enumerate(indices):
        #     self.blocks[p][s].sample_feature_labels_to(
        #         offset,
        #         inputs, policy_labels, value_labels, aux_labels)
        return (torch.from_numpy(inputs.reshape(N, -1, 9, 9)),
                torch.from_numpy(policy_labels),
                torch.from_numpy(value_labels),
                torch.from_numpy(aux_labels.reshape(N, -1)),
                torch.from_numpy(inputs2.reshape(N, -1, 9, 9)),
                torch.from_numpy(legalmove_labels.reshape(N, -1)),
                )


def load_torch_dataset(path: str | list[str]) -> torch.utils.data.Dataset:
    """load dataset from file"""
    if isinstance(path, list) or path.endswith('.sfen') \
       or path.endswith('.txt') or path.endswith('sfen.npz'):
        if not isinstance(path, list):
            path = [path]
        import tqdm
        with tqdm.tqdm(total=len(path), disable=(len(path) < 10)) as pbar:
            blocks = []
            pbar.set_description('loading blocks')
            for _ in path:
                blocks.append(miniosl.GameDataset.make_block(
                    _, compress_and_rm=False, strict=False))
                pbar.update(1)
        sizes = [len(_) for _ in blocks]
        unit = min(sizes)
        logging.info(f'load {sum(sizes)} data as {unit} x {len(sizes)}')
        set = GameDataset(unit * len(sizes), unit, batch_with_collate=True)
        for blk in blocks:
            set.add(blk)
        return set
    raise ValueError(f'unsupported data {path}')


"""input features (obs, obs_after) and labels (others)"""
BatchInput = recordclass.recordclass(
    'BatchInput', (
        'obs', 'move', 'value', 'aux',
        'obs_after',  'legal_move'))


BatchOutput = recordclass.recordclass(
    'BatchOutput', ('move', 'value', 'aux'))


def preprocess_batch(batch: torch.Tensor):
    """unpack data to feed them into neuralnetworks

    batch should be located on gpus in advance for efficient data transfer
    """
    inputs = BatchInput(*batch)
    inputs.obs = inputs.obs.float()
    inputs.aux = inputs.aux.float()
    inputs.obs_after = inputs.obs_after.float()
    inputs.obs /= miniosl.One
    inputs.move = inputs.move.long()
    inputs.aux /= miniosl.One
    inputs.obs_after /= miniosl.One
    return inputs


TargetValue = recordclass.recordclass(
    'TargetValue', ('td1', 'td2', 'soft'))


def process_target_batch(batch_output: tuple[torch.Tensor]):
    _, succ_value, *_ = batch_output
    succ_value = succ_value.float()
    ret = TargetValue(
        -succ_value[:, 0].flatten().detach(),  # tdtarget
        -succ_value[:, 1].flatten().detach(),  # tdtarget2
        -succ_value[:, 2].flatten().detach(),  # softtarget
    )
    return ret
