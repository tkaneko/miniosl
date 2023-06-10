import miniosl
import miniosl.network
import argparse
import logging
import torch
import numpy as np
import json
import random


FORMAT = '%(asctime)s %(levelname)s %(message)s'
logging.basicConfig(format=FORMAT, level=logging.DEBUG, force=True)

parser = argparse.ArgumentParser("training-by-torch.py")
parser.add_argument("--train-npz", help="npz filename for training",
                    default="positions")
parser.add_argument("--savefile", help="filename of output", default='out')
parser.add_argument("--loadfile", help="load filename", default='')
parser.add_argument("--device", help="torch device", default="cpu")
parser.add_argument("--batch-size", help="minibatch size", type=int, default=4096)
parser.add_argument("--validate-npz", help="npz filename for validation",
                    default="")
parser.add_argument("--validation-size", help="number of batches",
                    default=10)
parser.add_argument("--epoch-limit", help="maximum epoch", type=int, default=2)
parser.add_argument("--step-limit", help="maximum #updates, 0 for inf",
                    type=int, default=100000)
parser.add_argument("--report-interval", type=int, default=200)
parser.add_argument("--validation-interval", type=int, default=1000)
parser.add_argument("--inspect", help="visualize instance", action='store_true')
args = parser.parse_args()


class MoveDestination(torch.utils.data.Dataset):
    """dataset for a prediction task of the move destination given position.

    Each instance is
    - stored: 256bit binary of position and label pair
    - input: 44 channels of 9x9 board
      - 14 for white pieces: [ppawn, plance, pknight, psilver, pbishop, prook,
        king, gold, pawn, lance, knight, silver, bishop, rook]
      - 2 for empty and ones
      - 14 for black pieces
      - 14 for hand pieces
    - output: [0,80] for (y-1)*9+x-1

    This is for demonstration of collaboration between torch and miniosl.
    See collate_fn to improve efficiency.
    """
    CHANNELS = 44

    def __init__(self, filename):
        dict = np.load(filename)
        keys = list(dict.keys())
        if len(keys) != 1:
            logging.warning(f'expect single key {keys}')
        self.data = dict[keys[0]]
        logging.info(f'load {len(self.data)} data of {keys[0]} in {filename}')

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        cxx = True
        return self.getitem_in_cxx(idx) if cxx else self.getitem_in_python(idx)

    def getitem_in_cxx(self, idx):
        item = miniosl.to_state_label_tuple(self.data[idx])
        label = item.move.dst().index81()
        input = item.state.to_np_44ch().reshape(-1, 9, 9)
        return torch.from_numpy(input), label

    def getitem_in_python(self, idx):
        F = torch.nn.functional
        item = miniosl.to_state_label_tuple(self.data[idx])
        position = torch.Tensor(item.state.to_np()).long()
        offset = 14             # ptype*2 = 16 - 2 for empty and edge of white
        bd_channels = F.one_hot(position+offset, num_classes=30)
        bd_channels = bd_channels.transpose(1, 0).reshape(-1, 9, 9)
        bd_channels[int(miniosl.edge)+offset, :, :] = 1
        hand_pieces = torch.Tensor(item.state.to_np_hand()[:, None])
        hand_channels = hand_pieces.expand(-1, 81).reshape(-1, 9, 9)
        label = item.move.dst().index81()
        return torch.cat((bd_channels, hand_channels)), label

    def raw_data(self, idx):
        return miniosl.to_state_label_tuple(self.data[idx])


class BatchMoveDestination(MoveDestination):
    """batch version of MoveDestination"""

    def __init__(self, filename):
        super().__init__(filename)

    def __getitem__(self, idx):
        return self.data[idx]

    def collate(batch_tuple) -> torch.Tensor:
        inputs, labels = miniosl.to_np_batch_44ch(batch_tuple)
        inputs_reshaped = inputs.reshape(-1, MoveDestination.CHANNELS, 9, 9)
        return torch.from_numpy(inputs_reshaped), torch.from_numpy(labels).long()


def validate(model, loader, cfg):
    if loader is None:
        return

    model.eval()
    running_vloss = 0
    count = 0
    top1 = 0
    with torch.no_grad():
        for i, data in enumerate(loader):
            inputs, labels = data[0].to(cfg.device), data[1].to(cfg.device)
            outputs = model(inputs)
            top1 += torch.sum(torch.argmax(outputs, dim=1) == labels) \
                / len(labels)
            loss = torch.nn.functional.cross_entropy(outputs, labels)
            running_vloss += loss.item()
            count += 1
            if cfg.validation_size > 0 and i >= cfg.validation_size:
                break
    logging.info(f'validation loss: {running_vloss / count:.3f}, ' +
                 f'top1: {top1 / count:.3f}')

    return running_vloss


def at_the_end_of_interval(i, interval):
    return i % interval == interval - 1


def train(model, cfg):
    """sample of training framework, following pytorch tutorial

    https://pytorch.org/tutorials/beginner/introyt/trainingyt.html
    https://pytorch.org/tutorials/beginner/blitz/cifar10_tutorial.html
    """
    logging.info('start training')
    dataset = BatchMoveDestination(args.train_npz) # MoveDestination(args.train_npz)
    v_dataset = MoveDestination(args.validate_npz) if args.validate_npz else None

    criterion = torch.nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters())

    loader_class = torch.utils.data.DataLoader
    trainloader = loader_class(dataset, batch_size=cfg.batch_size,
                               collate_fn=BatchMoveDestination.collate,
                               shuffle=True, num_workers=0)
    validate_loader = None if v_dataset is None \
        else loader_class(v_dataset, batch_size=cfg.batch_size, shuffle=True,
                          num_workers=0)

    for epoch in range(cfg.epoch_limit):
        logging.info(f'start epoch {epoch}')
        validate(model, validate_loader, args)
        running_loss = 0.0
        for i, data in enumerate(trainloader, 0):
            model.train(True)
            inputs, labels = data[0].to(cfg.device), data[1].to(cfg.device)
            optimizer.zero_grad()

            outputs = model(inputs)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            running_loss += loss.item()
            if at_the_end_of_interval(i, cfg.report_interval):
                logging.info(f'[{epoch + 1}, {i + 1:5d}] ' +
                             f'loss: {running_loss / cfg.report_interval:.3f}')
                running_loss = 0.0
            if at_the_end_of_interval(i, cfg.validation_interval):
                validate(model, validate_loader, args)
            if cfg.step_limit > 0 and i >= cfg.step_limit:
                logging.warning('reach step_limit')
                break

    logging.info('finish')
    torch.save(model.state_dict(), f'{args.savefile}.pt')


def visualize_instance(model, dataset, cfg):
    id = random.randrange(len(dataset))
    input, label = dataset[id]
    logit = model(input.to(cfg.device))[0].detach()
    out = torch.softmax(logit, 0).to('cpu')
    state = dataset.raw_data(id).state
    move = dataset.raw_data(id).move
    out_channel = out.view(9, 9).numpy().copy()
    max = np.max(out_channel)
    out_channel /= max

    outfile = f'{args.savefile}-{id}.txt'
    with open(outfile, 'w') as file:
        print('id = ', id, file=file)
        print(move.to_csa(), f'{out[move.dst().index81()]*100:.2f}%',
              file=file)
        print(out, file=file)

    state.to_png(plane=out_channel,
                 filename=f'{args.savefile}-{id}.png')

    logging.info(f'wrote {outfile} and png')


def inspect(model, cfg):
    v_dataset = MoveDestination(args.validate_npz)
    validate_loader = torch.utils.data.DataLoader(v_dataset,
                                                  batch_size=cfg.batch_size,
                                                  shuffle=True, num_workers=0)
    validate(model, validate_loader, args)
    visualize_instance(model, v_dataset, args)


if __name__ == "__main__":
    with open(f'{args.savefile}.json', 'w') as file:
        json.dump(args.__dict__, file)

    network_cfg = {'in_channels': MoveDestination.CHANNELS,
                   'channels': 64, 'out_channels': 1}
    model = miniosl.network.BasicNetwork(**network_cfg).to(args.device)
    if args.loadfile:
        logging.info(f'load {args.loadfile}')
        saved_state = torch.load(args.loadfile,
                                 map_location=torch.device(args.device))
        model.load_state_dict(saved_state)

    if args.inspect:
        inspect(model, args)
    else:
        train(model, args)

# Local Variables:
# python-indent-offset: 4
# End:
