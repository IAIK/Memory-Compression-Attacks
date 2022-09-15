# POSTGRES ATTACK

Install the requirements with:

`pip3 install -r requirements.txt`

Start the server with:
`./server/run.sh`

The `TEMPLATE` file can already be used to showcase the attack and leak bytewise the secret string `SECRET` which is defined in the `attack.py`.

Run the attack script with:
```
Usage: attack.py [OPTIONS] TEMPLATE_PATH

Options:
  -h, --hostname TEXT
  -p, --port INTEGER
  -i, --interface TEXT
  -o, --output PATH
  --help                Show this message and exit
```

You can find a demo video here:
[Postgres Exploit](https://www.youtube.com/watch?v=3-ZBetgh04c&ab_channel=MartinSchwarzl)