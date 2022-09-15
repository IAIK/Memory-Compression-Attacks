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
def main(file):

    df = pd.read_csv(file)
    fig, ax = plt.subplots()
    dictionary = df["Secret"].unique().tolist()
    # Plot histograms
    palette = dict(zip(dictionary, sns.color_palette("Spectral", n_colors=len(dictionary))))
    histplot = sns.histplot(df, ax=ax, x=TIMING_SOURCE, hue="Secret", palette=palette, element="step", kde=True)
    medians = df.groupby(["Secret"])[TIMING_SOURCE].median()
    mins = df.groupby(["Secret"])[TIMING_SOURCE].min()
    stderr = df.groupby(["Secret"])[TIMING_SOURCE].sem()
     
    medians.to_csv('medians.csv')
    stderr.to_csv('stderr.csv')

if __name__ == "__main__":
    main()
