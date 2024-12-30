# miniOSL

Python interface to [OSL (open shogi library)](https://gps.tanaka.ecc.u-tokyo.ac.jp/gpsshogi/index.php?GPSshogi), refurbished and enhanced by C++20 with pybind11 for cooperation with numpy and pytorch. 

WIP.

## demo@colab

[https://colab.research.google.com/drive/1orT32kOR58owC0SfhPdC0yodFdn8h2nX?usp=share_link](https://colab.research.google.com/drive/1orT32kOR58owC0SfhPdC0yodFdn8h2nX?usp=share_link)

| `shogi.go(50)`            | `value, moves = shogi.eval()` |
|:-------------------------:|:-----------------------------:|
| ![](https://github.com/tkaneko/miniosl/raw/main/doc/board-sample.png) | ![](https://github.com/tkaneko/miniosl/raw/main/doc/eval-sample.png)      |

### integration with ipywidgets

(experimental)

![](https://github.com/tkaneko/miniosl/raw/main/doc/slider.png)

## pip wheel

- [https://pypi.org/project/miniosl/](https://pypi.org/project/miniosl/)

## work with source code

- C++20 (tested with g++ 12.2.1 in `manylinux_2_28`, Apple clang 14.0.3)
- Python 3.10+
- cmake 3.22+

typical commands
- `git clone --recursive https://github.com/tkaneko/miniosl.git`
- `cd miniosl`
- `pip3 install -e .`
  - example for envvar: `CMAKE_BUILD_PARALLEL_LEVEL=4` `CXX=g++-12`

### cui samples

- [shogiconvert](https://github.com/tkaneko/miniosl/blob/main/miniosl/utility/convert_record.py)
- [selfplay](https://github.com/tkaneko/miniosl/blob/main/miniosl/utility/selfplay.py)
- [shogiviewer](https://github.com/tkaneko/miniosl/blob/main/miniosl/utility/curses_viewer.py)

![](https://github.com/tkaneko/miniosl/raw/main/doc/term.svg)

## api doc

[api 0.1.3](https://game.c.u-tokyo.ac.jp/miniosl-api/0-1-3/)

The location is tentative and subject to change in the future.

