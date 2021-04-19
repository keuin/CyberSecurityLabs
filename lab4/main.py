import json
import logging
import re
from typing import Iterator, Tuple

import requests
from requests import Session
from url_normalize import url_normalize

LOG_FORMAT = '[%(levelname)s][%(asctime)-15s][%(filename)s][%(lineno)d] %(message)s'
logging.basicConfig(format=LOG_FORMAT, level='ERROR')

__url_matcher = re.compile(r'((?:https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|!:,.;]+[-A-Za-z0-9+&@#/%=~_|])')



def is_url_safe(url):
    for j in ['.zip', '.rar', '.7z', '.jpg', '.jpeg', '.png', '.gif', '.bmp', '.svg',
              '.doc', '.docx', '.ppt', '.pptx', '.xls', '.xlsx']:
        if url.lower().endswith(j):
            return False
    return True


def search_webpage(start_url: str, max_depth: int = 3, ua: str = None, __ses: Session = None, __depth=0,
                   __searched_pages=None) -> Iterator[Tuple[str, str]]:
    try:
        if not __ses:
            __ses = requests.Session()
            # if proxies:
            #     __ses.proxies.update(proxies)
            if ua:
                __ses.headers['User-Agent'] = ua
        if not __searched_pages:
            __searched_pages = dict()
        normal_url = url_normalize(start_url)
        if normal_url in __searched_pages:
            raise RuntimeError()

        r = __ses.get(start_url, timeout=2)
        r.encoding = r.apparent_encoding
        logging.info(f'Page {start_url} with detected encoding {r.encoding}')
        page_content = r.text
        yield start_url, page_content
        __searched_pages[normal_url] = None
        if __depth < max_depth:
            for url in filter(lambda u: u not in __searched_pages,
                              filter(is_url_safe, __url_matcher.findall(page_content))):
                logging.info(f'{start_url} -> {url}')
                yield from search_webpage(url, max_depth, __ses=__ses, __depth=__depth + 1,
                                          __searched_pages=__searched_pages)
    except IOError as e:
        logging.warning(f'IOError while reading `{start_url}`: {e}')
    except RuntimeError:
        pass
    if __depth == 0 and __ses:
        __ses.close()


user_agent = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 ' \
             '(KHTML, like Gecko) Chrome/89.0.4389.114 Safari/537.36'
if __name__ == '__main__':
    with open('keywords.json', 'r', encoding='utf-8') as f:
        keywords = {kw: 0 for kw in json.load(f)}
    start_url = input('Input initial URL:') or 'http://www.hit.edu.cn'
    max_depth = int(input('Search depth (default is 1):') or 1)
    with open('result.txt', 'w', encoding='utf-8') as f:
        for url, webpage in search_webpage(start_url, ua=user_agent, max_depth=max_depth):
            for kw in keywords.keys():
                if cnt := len(tuple(re.finditer(kw, webpage))):
                    keywords[kw] += cnt
                    msg = f'Webpage `{url}` contains keyword `{kw}` {cnt} {"time" if cnt < 2 else "times"}.'
                    print(msg)
                    f.write(msg)
                    f.write('\n')
        msg = '=' * 16 + '\nStatistics:\n' + '\n'.join('%s: %s' % ele for ele in keywords.items()) + '\n' + '=' * 16
        f.write(msg)
        print(msg)
