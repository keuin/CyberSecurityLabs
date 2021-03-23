#!/bin/python3

import logging
import re
import time
from threading import Thread
from typing import Iterator, Union

from connect_scanner import ScanTarget, ConcurrentConnectionScanner, FastWriteCounter
from main_frame import *
from util import get_time_string, inet4_subnet_generator

LOG_FORMAT = '[%(levelname)s][%(asctime)-15s][%(filename)s][%(lineno)d] %(message)s'
logging.basicConfig(format=LOG_FORMAT, level='INFO')

targets = dict()  # target name -> ScanTarget
scanner = None

logging.info('INFO is on')
logging.info('DEBUG is on')


class MainScanThread(Thread):
    """
    Convert the blocking scanner into an asynchronous one.
    """

    def __init__(self, callback, printer, timeout_seconds: Union[int, float] = 2, max_workers: int = 16,
                 counter_callback=None, scan_finished_callback=None):
        super().__init__()
        self.__callback = callback
        self.__printer = printer
        self.__timeout_seconds = timeout_seconds
        self.__max_workers = max_workers
        self.__counter_callback = counter_callback
        self.__scan_finished_callback = scan_finished_callback

    def run(self) -> None:
        def __targets() -> Iterator[ScanTarget]:
            for name, scan_target in targets.items():
                yield scan_target

        self.__scanner = ConcurrentConnectionScanner(__targets())
        t = time.time()
        count_all, count_open = self.__scanner.scan(callback=self.__callback,
                                                    timeout_seconds=self.__timeout_seconds,
                                                    max_workers=self.__max_workers,
                                                    finished_counter_provider=self.__counter_callback)
        t = time.time() - t
        self.__printer(f'Scan finished. '
                       f'{count_open} endpoints are open ({count_all} in total). \n'
                       f'Time elapsed: %.2fs. '
                       f'Average speed: %.2f try per second.' % (t, count_all / t))
        if self.__scan_finished_callback:
            self.__scan_finished_callback()


class ConcreteMainFrame(MainFrame):

    def __init__(self, *args, **kwds):
        super().__init__(*args, **kwds)
        self.SetMinSize((950, 540))
        self.SetSize((950, 540))
        self.__logger = logging.getLogger(ConcreteMainFrame.__name__)
        self.__scanner_thread = None
        self.__stopped = False
        self.__start_time = 0

    def button_remove_target_click(self, event):
        list_items = self.check_list_box_targets.GetItems()
        selected_items = self.check_list_box_targets.GetSelections()

        has_selected = False
        for target_id in selected_items:
            if self.check_list_box_targets.IsChecked(target_id):
                has_selected = True
                target_name = list_items[target_id]
                self.check_list_box_targets.Delete(target_id)
                targets.pop(target_name)
                self.__logger.info(f'Removed target {target_name}')

        if not has_selected:
            wx.MessageBox('Please select targets to be deleted.')
            return

    def button_add_target_click(self, event):
        scan_name = str(self.text_name.Value).strip()
        scan_host = str(self.text_host.Value).strip()
        scan_port = str(self.text_port.Value).strip()

        if re.fullmatch(r'[0-9]*', scan_host):
            wx.MessageBox(f'Numeric host "{scan_host}" is not allowed.', 'Invalid host')
            return

        pattern_inet4_addr = r'^((25[0-5]|(2[0-4]|1[0-9]|[1-9]|)[0-9])(\.(?!$)|$)){4}$'
        pattern_inet4_subn = r'^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}/(8|16|24)$'
        if re.fullmatch(r'[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*', scan_host) \
                and not re.fullmatch(pattern_inet4_addr, scan_host):
            wx.MessageBox(f'Invalid INET4 address "{scan_host}".\n'
                          f'Mismatched with r"{pattern_inet4_addr}".')
            return

        if not scan_name:
            wx.MessageBox('Target name cannot be empty.')
            return

        ### validate host
        valid_host = True
        if not scan_host:
            valid_host = False
        else:
            for c in ['*', '?', '%', '#', '@', '%', '(', ')']:
                if c in scan_host:
                    valid_host = False
                    break
        if not valid_host:
            wx.MessageBox(f'Invalid host {scan_host}!')
            return

        ### validate port
        valid_port = True
        if not scan_port:
            valid_port = False
        else:
            if re.fullmatch(r'[0-9]{1,5}', scan_port) and int(scan_port) in range(1, 65536):
                # single port
                valid_port = True
                scan_port = [int(scan_port)]
            elif re.fullmatch(r'[0-9]{1,5}-[0-9]{1,5}', scan_port):
                s = scan_port.split('-')
                if int(s[0]) in range(1, 65536) and int(s[1]) in range(1, 65536):
                    valid_port = True
                    scan_port = range(int(s[0]), int(s[1]) + 1)
                else:
                    valid_port = False
            else:
                valid_port = False
        if not valid_port:
            wx.MessageBox(f'Invalid port {scan_port}!')
            return

        if scan_name in targets:
            wx.MessageBox(f"Target with name `{scan_name}` already exists!")
            return

        self.__logger.info(f'Add target {scan_name}, host {scan_host}, port {scan_port}')

        if re.fullmatch(pattern_inet4_subn, scan_host):
            # a INET4 subnet
            scan_host_generator = inet4_subnet_generator(scan_host)
        else:
            # a single INET4 address
            scan_host_generator = [scan_host]
        targets[scan_name] = ScanTarget(scan_host_generator, scan_port, scan_name)
        self.check_list_box_targets.Append(scan_name)

    def button_save_logs_click(self, event):
        self.__logger.info('Save logs')
        with wx.FileDialog(self, 'Save log to file', wildcard='Text files (*.txt)|*.txt',
                           style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT) as file_dialog:
            if file_dialog.ShowModal() == wx.ID_CANCEL:
                return  # user cancelled
            file_path = file_dialog.GetPath()
            val = self.text_output.Value
        try:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(val)
        except IOError as e:
            wx.MessageBox(f'Unexpected exception occurred while saving logs to file: {e}',
                          'Failed to save')
            return
        wx.MessageBox(f'Logs saved as file {file_path}')

    def button_start_scan_click(self, event):
        if self.__scanner_thread and self.__scanner_thread.is_alive():
            wx.MessageBox('Scanning is already started.')
            return
        ## Validate targets
        if not targets:
            wx.MessageBox('No scan target specified. Please add at least one scan target.\n '
                          'Scan target is consisted with one host string and a port/port range.'
                          'Such as: "192.168.1.1 1-65535" or "10.0.0.1 80".',
                          'No scan target')
            return
        ## Validate max workers
        max_workers = str(self.spin_ctrl_threads.Value)
        if not re.fullmatch('[0-9]+', max_workers):
            wx.MessageBox(f'Invalid workers count. Must be a positive integer.')
            return
        max_workers = int(max_workers)

        ## Validate timeout
        timeout_ms = str(self.spin_ctrl_timeout.Value)
        if not re.fullmatch(r'[0-9]*\.?[0-9]*', timeout_ms):
            wx.MessageBox(f'Invalid timeout "{timeout_ms}": must be an integer or float number.')
            return
        timeout_ms = float(timeout_ms)
        self.__logger.debug(f'Timeout is set to {timeout_ms} ms.')

        ## Start scan thread
        self.__stopped = False
        self.__scanner_thread = MainScanThread(self.__scan_callback,
                                               max_workers=max_workers,
                                               printer=self.print,
                                               timeout_seconds=timeout_ms / 1000.0,
                                               counter_callback=self.__counter_callback,
                                               scan_finished_callback=self.__scan_finished_callback)
        self.__scanner_thread.start()
        self.__start_time = time.time()
        self.__logger.info('Start scanning')
        self.clear()
        self.print(f'Scan started ({max_workers} worker{"s" if (max_workers > 1) else ""}).')
        Thread(target=self.__update_ui).start()  # UI update thread

    def print(self, s: str):
        wx.CallAfter(self.text_output.AppendText, f'[{get_time_string()}] {s}\n')

    def update_status_bar(self, s: str):
        wx.CallAfter(self.label_status.SetLabelText, s)

    def clear(self):
        wx.CallAfter(self.text_output.Clear)

    def __scan_callback(self, host: str, port: int, name: str):
        self.print(f'Endpoint [{name}] {host}:{port} is open.')

    def __counter_callback(self, counter: FastWriteCounter):
        self.__counter = counter

    def __update_ui(self):
        while not self.__stopped:
            time.sleep(1)
            if not self.__counter:
                continue
            count_scanned = self.__counter.value()
            time_elapsed = time.time() - self.__start_time
            self.update_status_bar(f'{count_scanned} endpoints scanned. Speed: %.2f tps.'
                                   % (count_scanned / time_elapsed))
        self.update_status_bar(f'Scan finished.')

    def __scan_finished_callback(self):
        self.__stopped = True


class KNetScannerGUI(wx.App):
    def OnInit(self):
        self.frame_main = ConcreteMainFrame(None, wx.ID_ANY, "")
        self.SetTopWindow(self.frame_main)
        self.frame_main.Show()
        return True


# end of class KNetScannerGUI

if __name__ == "__main__":
    knetscanner = KNetScannerGUI(0)
    knetscanner.MainLoop()
