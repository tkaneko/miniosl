import miniosl
import torch
import numpy as np
import logging


class StandardDataset(torch.utils.data.Dataset):
    def __init__(self, filename):
        dict = np.load(filename)
        keys = list(dict.keys())
        if len(keys) != 1:
            logging.warning(f'expect single key {keys}')
        self.data = dict[keys[0]]
        logging.info(f'load {len(self.data)} data {keys[0]} from {filename}')

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        return self.policy_item_with_aux(idx)

    def policy_item_with_aux(self, idx):
        item = miniosl.to_state_label_tuple320(self.data[idx])
        input, move_label, value_label, aux_label = item.to_np_feature_labels()
        input = input.reshape(-1, 9, 9)
        return torch.from_numpy(input), move_label, np.float32(value_label), torch.from_numpy(aux_label)

    def raw_data320(self, idx):
        return miniosl.to_state_label_tuple320(self.data[idx])
