import mysql.connector
import os

import scheme

db = mysql.connector.connect(
    host='localhost',
    port=3306,
    user='root',
    password='Mys9l'
)

cur = db.cursor()
cur.execute('CREATE DATABASE IF NOT EXISTS lab5')
cur.execute('USE lab5')
cur.execute('DROP TABLE user')
cur.execute('''
CREATE TABLE IF NOT EXISTS user (
    username VARCHAR(32),
    password VARCHAR(128)
)
'''.strip())
# cur.execute('DELETE * FROM user')
username = input('Username:')
password = input('Password:')
password = scheme.sha512(username + password)
cur.execute(f'INSERT INTO user (username, password) VALUES ("{username}", "{password}")')
db.commit()
