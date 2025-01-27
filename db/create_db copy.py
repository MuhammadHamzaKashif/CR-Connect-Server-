import sqlite3
import pandas as pd

data = pd.read_csv('msg.csv')
lim = len(data)

connection = sqlite3.connect('msg.db')
cursor = connection.cursor()

cursor.execute('''
CREATE TABLE IF NOT EXISTS questions (
    sender TEXT,
    receiver TEXT,
    messages TEXT
)
''')
for i in range(lim):
    cursor.execute('INSERT INTO questions (sender, receiver, messages (?, ?, ?)',
               (data.iloc[i].sender, data.iloc[i].receiver, data.iloc[i].messages))

connection.commit()
connection.close()
