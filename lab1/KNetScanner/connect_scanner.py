import itertools
import socket
import logging
import threading
from concurrent.futures.thread import ThreadPoolExecutor
from threading import Thread
from typing import Iterator, Union
from queue import Queue
import time


class FastWriteCounter(object):
    def __init__(self):
        self._number_of_read = 0
        self._counter = itertools.count()
        self._read_lock = threading.Lock()

    def increment(self):
        next(self._counter)

    def value(self) -> int:
        with self._read_lock:
            value = next(self._counter) - self._number_of_read
            self._number_of_read += 1
        return value


class ScanTarget:
    """
    A scan target defines a range of host and port.
    """

    def __init__(self, hosts: Union[Iterator[str], list[str]],
                 ports: Union[Iterator[int], list[int]],
                 name: str = None):
        assert isinstance(hosts, Iterator) or isinstance(hosts, list) or isinstance(hosts, range), \
            f'Bad hosts: {hosts}'
        assert isinstance(ports, Iterator) or isinstance(ports, list) or isinstance(ports, range), \
            f'Bad ports: {ports}'
        self.__hosts = hosts
        self.__ports = ports
        self.__name = name

    def iterate(self) -> Iterator[tuple[str, int]]:
        for host in self.__hosts:
            for port in self.__ports:
                yield host, port

    @property
    def name(self):
        return self.__name

    def __str__(self) -> str:
        if name := self.__name:
            return f'<ScanTarget {name}, host={self.__hosts}, port={self.__ports}>'
        return super().__str__()


class ConcurrentConnectionScanner:
    """
    A scanner utilizing TCP connect() method,
    which supports multithreading scanning.
    """

    def __init__(self, scan_target: Union[ScanTarget, Iterator[ScanTarget]] = None):
        if isinstance(scan_target, ScanTarget):
            self.__target = [scan_target]
        elif isinstance(scan_target, Iterator):
            self.__target = list(scan_target)
        elif scan_target:
            raise TypeError('Bad scan_target')
        else:
            self.__target = []
        self.__logger = logging.getLogger(ScanTarget.__name__)

    def add_target(self, scan_target: Union[ScanTarget, Iterator[ScanTarget]]):
        if isinstance(scan_target, ScanTarget):
            self.__target.append(scan_target)
        elif isinstance(scan_target, Iterator):
            self.__target += list(scan_target)
        else:
            raise TypeError('Bad scan_target')

    def scan(self, callback, timeout_seconds=2, max_workers=16) -> (int, int):
        """
        Start a blocking multi-threaded scan.
        :param callback: receives a `host`, a `port` and a `name`
        :param timeout_seconds: timeout for socket CONNECT() method.
        :param max_workers: how many threads to use at most.
        :return (count_scanned, count_open)
        """
        if not isinstance(timeout_seconds, int) and not isinstance(timeout_seconds, float):
            raise TypeError('Bad timeout_seconds')

        def __scan(host: str, port: int, name: str, que: Queue, open_counter: FastWriteCounter):
            try:
                r = socket.create_connection((host, port), timeout=timeout_seconds)
            except Exception as e:
                del r
                self.__logger.debug(f'Target {host}:{port} timed out: {e}')
                return False
            del r
            self.__logger.debug(f'Target {host}:{port} is open.')
            que.put((host, port, name))
            open_counter.increment()
            return True

        def __callback(cb, que: Queue):
            self.__logger.info('Callback invoker thread started.')
            while True:
                h, p, name = que.get()
                if not h:
                    break
                cb(host=h, port=p, name=name)
            self.__logger.info('Callback invoker thread stopped.')

        self.__logger.info('Start scanning...')
        q = Queue(maxsize=8192)
        counter_scanned = 0
        counter_open = FastWriteCounter()
        try:
            Thread(target=__callback, kwargs={'cb': callback, 'que': q}).start()
            with ThreadPoolExecutor(max_workers=max_workers) as executor:
                for target in self.__target:
                    target: ScanTarget
                    self.__logger.info(f'Submit scan target {target}.')
                    for host, port in target.iterate():
                        assert isinstance(host, str), f'bad host {host}'
                        assert isinstance(port, int), f'bad port {port}'
                        executor.submit(__scan, host=host, port=port, name=target.name, que=q, open_counter=counter_open)
                        counter_scanned += 1
                executor.shutdown(wait=True, cancel_futures=False)
        except Exception as e:
            print(f'Unexpected exception: {e}')
        q.put((None, None, None))
        return counter_scanned, counter_open.value()


def simple_callback(host: str, port: int, name: str):
    assert isinstance(host, str)
    assert isinstance(port, int)
    print(f'Opened endpoint: [{name}] {host}:{port}')


if __name__ == '__main__':
    print('Start scanning...')
    target = ScanTarget(hosts=['127.0.0.1'], ports=range(1, 65536), name='testname')
    scanner = ConcurrentConnectionScanner(scan_target=target)
    t = time.time()
    count_all, count_open = scanner.scan(simple_callback, max_workers=1)
    t = time.time() - t
    print(f'Scan finished. '
          f'{count_open} endpoints are open ({count_all} in total). '
          f'Time elapsed: %.2fs. '
          f'Average speed: %.2f try per second.' % (t, count_all / t))
