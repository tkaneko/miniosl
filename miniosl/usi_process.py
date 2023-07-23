"""run external usi engine"""
import subprocess
import re
import logging
import copy


info_string = re.compile(r'^info\s+string.*$')
info_regexp = re.compile(r'^info.*\s+score\s+cp\s+([0-9-+]+).*\s+pv\s+((\s?[A-Za-z0-9+*]+)*)\s*$')
depth_regexp = re.compile(r'^info.*\s+depth\s+([0-9]+)')
nodes_regexp = re.compile(r'^info.*\s+nodes\s+([0-9]+)')
bestmove_regexp = re.compile(r'^bestmove\s+([A-Za-z0-9+*]+)\s*(ponder\s.*)*$')
idname_regexp = re.compile(r'^id\s+name\s+(.*)\s*$')


class UsiProcess:
    """Proxy for usi process

    create process, initilaze its options and wait until ``readyok``

    :param path_and_args: list of path and argments
    """
    debug = False

    def __init__(self, path_and_args: list[str],
                 setoptions: list[str] = [
                   "setoption name Threads value 1",
                   "setoption name USI_Hash value 16",
                 ],
                 cwd: str | None = None):
        self.path_and_args = copy.copy(path_and_args)
        self.name = None
        self.usi = subprocess.Popen(path_and_args, bufsize=1,
                                    stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE,
                                    cwd=cwd,
                                    close_fds=True, universal_newlines=True)
        self.writeline("usi")
        line = self.readline()
        options_avail = []
        while line != "usiok":
            if line.startswith("option name"):
                options_avail.append(line)
            else:
                match = re.match(idname_regexp, line)
                if match:
                    self.name = match.group(1)
            line = self.readline()
        for line in setoptions:
            self.writeline(line)
        self.writeline("isready")
        ready = self.readline()
        while re.match(info_string, ready):
            ready = self.readline()
        if not ready.startswith("readyok"):
            err = f'readyok != {ready}'
            logging.critical(err)
            raise ValueError(err)

    def readline(self) -> str:
        line = self.usi.stdout.readline().rstrip()
        if self.debug:
            logging.debug(f'< {line}')
        return line

    def writeline(self, line) -> None:
        if self.debug:
            logging.debug(f'> {line}')
        self.usi.stdin.write(line+"\n")

    def close(self) -> bool:
        """finish process"""
        self.writeline("quit")
        self.usi.stdin.close()
        if self.usi.wait() != 0:
            print("close error")
            return False
        return True

    def search(self, position: str, limit: str) -> dict:
        """search: give position with go commands, and wait ``bestmove``

        Returns hash contains "info"

        :param position: e.g., ``'startpos'``
        :param limit: e.g., ``'byoyomi 1000'``
        """
        self.writeline(f'position {position}')
        self.writeline(f"go {limit}")
        result = {}
        last_depth = None
        last_node_count = None
        best_move_identified = False
        while not best_move_identified:
            line = self.readline()
            match_depth = re.match(depth_regexp, line)
            if match_depth:
                last_depth = match_depth.group(1)
            match_nodes = re.match(nodes_regexp, line)
            if match_nodes:
                last_node_count = match_nodes.group(1)
            match_info = False if 'rep' in line else re.match(info_regexp, line)
            if match_info:
                score = match_info.group(1)
                pv = match_info.group(2)
                result['cp'] = score
                result['pv'] = pv
                if last_depth:
                    result['d'] = last_depth
                if last_node_count:
                    result['n'] = last_node_count
            match_bestmove = re.match(bestmove_regexp, line)
            if match_bestmove:
                result['move'] = match_bestmove.group(1)
                best_move_identified = True
        return result
