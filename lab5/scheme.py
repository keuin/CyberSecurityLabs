import time
from typing import Union
import os

from Crypto.Hash import SHA256, SHA512
from Crypto.Cipher import AES


def sha512(s: Union[str, bytes], string=True):
    if isinstance(s, str):
        s = s.encode('utf-8')
    o = SHA512.new(s)
    if string:
        return o.hexdigest()
    else:
        return o.digest()


def sha256(s: Union[str, bytes], string=True):
    if isinstance(s, str):
        s = s.encode('utf-8')
    o = SHA256.new(s)
    if string:
        return o.hexdigest()
    else:
        return o.digest()


def password_hash(username, password, salt):
    return sha512(sha512(username + password) + salt)


def create_login_request(username: str, password: str, salt: Union[str, bytes] = None) -> dict:
    if not salt:
        salt = os.urandom(16).hex()
    if isinstance(salt, bytes):
        salt = salt.hex()
    assert isinstance(username, str)
    assert isinstance(password, str)
    assert isinstance(salt, str)
    return {
        'user': username,
        'request': password_hash(username, password, salt),
        'code': salt
    }


def validate_user(client_request: dict, userpass_hash: str) -> bool:
    """
    Validate user password.
    :param client_request: the client's raw request json.
    :param userpass_hash: the hash stored in the database.
    :return: if the user is validated.
    """
    salt = client_request.get('code')
    actual = client_request.get('request')
    if not isinstance(salt, str) or len(salt) != 32:
        raise ValidationError('bad salt')
    if not isinstance(actual, str) or len(actual) != 128:
        raise ValidationError(f'bad sha512 request: {actual}')
    expected = sha512(userpass_hash + salt)
    # fixed time comparison
    acc = 0
    for j, k in zip(actual, expected):
        acc += (ord(j) - ord(k)) ** 2
    return not acc


def encrypt(data: bytes, k: Union[str, bytes]) -> (bytes, bytes):
    if isinstance(k, str):
        k = k.encode('utf-8')
    if not isinstance(k, bytes):
        raise ValueError(f'bad key type: {type(k)}')
    if isinstance(data, str):
        data = data.encode('utf-8')
    if not isinstance(data, bytes):
        raise ValueError(f'bad data type: {type(data)}')
    iv = os.urandom(16)
    return iv, AES.new(key=sha256(k, string=False), mode=AES.MODE_CBC, iv=iv).encrypt(data)


def decrypt(data: bytes, k: Union[str, bytes], iv: bytes) -> (bytes, bytes):
    if isinstance(k, str):
        k = k.encode('utf-8')
    return AES.new(key=sha256(k, string=False), mode=AES.MODE_CBC, iv=iv).decrypt(data)


class ValidationError(Exception):
    pass
