"""House style for the measurement plots (local PNGs in measurements/): thin lines, recessive grid, fixed
categorical order (the dataviz reference palette), legend whenever there are two or more series, one y-axis."""
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

SERIES = ['#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4', '#008300', '#6250d6', '#e34948']
SURFACE, INK, INK2, GRID = '#fcfcfb', '#0b0b0b', '#52514e', '#e4e3df'

plt.rcParams.update({
    'figure.facecolor': SURFACE, 'axes.facecolor': SURFACE, 'savefig.facecolor': SURFACE,
    'axes.edgecolor': GRID, 'axes.labelcolor': INK2, 'xtick.color': INK2, 'ytick.color': INK2,
    'text.color': INK, 'axes.grid': True, 'grid.color': GRID, 'grid.linewidth': 0.8,
    'axes.spines.top': False, 'axes.spines.right': False, 'lines.linewidth': 2.0,
    'axes.prop_cycle': matplotlib.cycler(color=SERIES), 'font.size': 10, 'legend.frameon': False,
    'figure.dpi': 110,
})


def fig(title, xlabel, ylabel, logx=False, size=(8, 4.5)):
    f, ax = plt.subplots(figsize=size)
    ax.set_title(title, loc='left', fontsize=12, color=INK)
    ax.set_xlabel(xlabel); ax.set_ylabel(ylabel)
    if logx: ax.set_xscale('log')
    return f, ax


def save(f, path, legend=True):
    ax = f.axes[0]
    if legend and len(ax.get_lines()) >= 2 and any(not l.get_label().startswith('_') for l in ax.get_lines()):
        ax.legend(loc='best', fontsize=9)
    f.tight_layout()
    f.savefig(path)
    plt.close(f)
