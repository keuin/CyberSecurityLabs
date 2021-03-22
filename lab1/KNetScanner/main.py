import logging
import time
from functools import partial
from threading import Thread
from typing import Iterator

import wx
import re
from main_frame import *
from connection_scanner import ScanTarget, ConcurrentConnectionScanner
from util import get_time_string

LOG_FORMAT = '[%(levelname)s][%(asctime)-15s][%(filename)s][%(lineno)d] %(message)s'
logging.basicConfig(format=LOG_FORMAT, level='DEBUG')

targets = dict()  # target name -> ScanTarget
scanner = None


class MainScanThread(Thread):
    """
    Convert the blocking scanner into an asynchronous one.
    """

    def __init__(self, callback, printer, timeout_seconds: int = 2, max_workers: int = 16):
        super().__init__()
        self.__callback = callback
        self.__printer = printer
        self.__timeout_seconds = timeout_seconds
        self.__max_workers = max_workers

    def run(self) -> None:
        def __targets() -> Iterator[ScanTarget]:
            for name, scan_target in targets.items():
                yield scan_target

        self.__scanner = ConcurrentConnectionScanner(__targets())
        t = time.time()
        count_all, count_open = self.__scanner.scan(callback=self.__callback,
                                                    timeout_seconds=self.__timeout_seconds,
                                                    max_workers=self.__max_workers)
        t = time.time() - t
        self.__printer(f'Scan finished. '
                       f'{count_open} endpoints are open ({count_all} in total). \n'
                       f'Time elapsed: %.2fs. '
                       f'Average speed: %.2f try per second.' % (t, count_all / t))


class ConcreteMainFrame(MainFrame):

    def __init__(self, *args, **kwds):
        super().__init__(*args, **kwds)
        self.__logger = logging.getLogger(ConcreteMainFrame.__name__)
        self.__scanner_thread = None

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
            for c in ['/', '*', '?', '%', '#', '@', '%', '(', ')']:
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
                scan_port = [valid_port]
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
        targets[scan_name] = ScanTarget([scan_host], scan_port, scan_name)
        self.check_list_box_targets.Append(scan_name)

    def button_save_logs_click(self, event):
        self.__logger.info('Save logs')
        with wx.FileDialog(self, 'Save log to file', wildcard='Text files (*.txt)|*.txt',
                           style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT) as file_dialog:
            if file_dialog.ShowModal() == wx.ID_CANCEL:
                return  # user cancelled
            file_path = file_dialog.GetPath()
            try:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(self.text_output.Value)
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

        ## Start scan thread
        self.__scanner_thread = MainScanThread(self.__scan_callback,
                                               max_workers=max_workers,
                                               printer=self.print)
        self.__scanner_thread.start()
        self.__logger.info('Start scanning')
        self.clear()
        self.print(f'Scan started ({max_workers} worker{"s" if (max_workers > 1) else ""}).')

    def print(self, s: str):
        self.text_output.AppendText(f'[{get_time_string()}] {s}\n')

    def clear(self):
        self.text_output.Clear()

    def __scan_callback(self, host: str, port: int, name: str):
        self.print(f'Endpoint [{name}] {host}:{port} is open.')


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
