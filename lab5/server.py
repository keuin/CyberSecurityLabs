import json
import traceback

import uvicorn
from pydantic import BaseModel
import mysql.connector
import scheme
from fastapi import FastAPI

app = FastAPI()

db = mysql.connector.connect(
    host='localhost',
    port=3306,
    user='root',
    password='Mys9l'
)


def get_hashed_password(user):
    with db.cursor() as cur:
        cur.execute('USE lab5')
        cur.execute('SELECT password FROM user WHERE username=%s', (user,))
        for pwd in cur:
            return pwd[0]


class LoginModel(BaseModel):
    user: str
    request: str
    code: str

    def get_dict(self) -> dict:
        return {
            'user': self.user,
            'request': self.request,
            'code': self.code
        }


@app.get("/")
def read_root():
    return 'Hello, world!'


@app.post("/login")
def read_item(req: LoginModel):
    try:
        username = req.user
        if not username:
            raise ValueError()
        pwd = get_hashed_password(username)
        if not pwd:  # user does not exist
            raise ValueError('user does not exist')
        if scheme.validate_user(req.get_dict(), pwd):
            return {
                'message': 'OK',
                'data': b''.join(scheme.encrypt(req.code.encode('utf-8'), pwd)).hex()
            }
    except:
        traceback.print_exc()
    return {'message': 'failed'}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
