#!/usr/bin/env python3

import numpy as np
import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import click

TIMING_SOURCE = 'Prev TS Frame'

@click.command()
@click.argument('file', type=click.Path(exists=True))
@click.option('-o', '--output', type=click.Path())
def main(file, output):

    df = pd.read_csv(file)
    fig, ax = plt.subplots()
    dictionary = df["Secret"].unique().tolist()
    # Plot histograms
    palette = dict(zip(dictionary, sns.color_palette("Spectral", n_colors=len(dictionary))))
    histplot = sns.histplot(df, ax=ax, x=TIMING_SOURCE, hue="Secret", palette=palette, element="step", kde=True)
    medians = df.groupby(["Secret"])[TIMING_SOURCE].median()
    mins = df.groupby(["Secret"])[TIMING_SOURCE].min()
    for word, value in medians.items():
        plt.axvline(value, linewidth=1, color=palette[word])
    # https://github.com/mwaskom/seaborn/issues/2280
    def move_legend(ax, **kws):
        old_legend = ax.legend_
        handles = old_legend.legendHandles
        labels = [t.get_text() for t in old_legend.get_texts()]
        title = old_legend.get_title().get_text()
        ax.legend(handles, labels, title=title, **kws)
    move_legend(histplot, bbox_to_anchor=(1.01, 1), loc=2, ncol=2,
                          borderaxespad=0)
    plt.subplots_adjust(left=0.05, right=0.88, top=0.92, bottom=0.1)
    # Show or store
    if output:
        my_dpi=96
        WIDTH = 1920
        HEIGHT = 1080
        fig.set_figheight(HEIGHT/my_dpi)
        fig.set_figwidth(WIDTH/my_dpi)
        median = df[TIMING_SOURCE].median()
        std = df[TIMING_SOURCE].std()
        plt.xlim([median - std, 2*median])
        plt.subplots_adjust(left=0.05, right=0.85, top=0.99, bottom=0.05)
        plt.savefig(output, dpi=300)
    else:
        plt.show()

if __name__ == "__main__":
    main()
