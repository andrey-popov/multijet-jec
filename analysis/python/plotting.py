import numpy as np

import matplotlib as mpl
from matplotlib import pyplot as plt


def plot_distribution(
    hist_data, hist_sim, x_label='', y_label='Events', era_label='', height_ratio=3.,
    mark_underflow=False
):
    """Plot distributions in data and simulation.
    
    Plot the two distributions and the deviations.  Under- and overflow
    bins of the histograms are ignored.
    """
    
    # Convert histograms to NumPy representations
    num_bins = hist_data.GetNbinsX()
    binning = np.zeros(num_bins + 1)
    bin_centres = np.zeros(num_bins)
    
    for bin in range(1, num_bins + 2):
        binning[bin - 1] = hist_data.GetBinLowEdge(bin)
    
    for i in range(len(bin_centres)):
        bin_centres[i] = (binning[i] + binning[i + 1]) / 2
    
    data_values, data_errors = np.zeros(num_bins), np.zeros(num_bins)
    sim_values, sim_errors = np.zeros(num_bins), np.zeros(num_bins)
    
    for bin in range(1, num_bins + 1):
        data_values[bin - 1] = hist_data.GetBinContent(bin)
        data_errors[bin - 1] = hist_data.GetBinError(bin)
        sim_values[bin - 1] = hist_sim.GetBinContent(bin)
        sim_errors[bin - 1] = hist_sim.GetBinError(bin)
    
    
    # Compute residuals, allowing for possible zero expectation
    residuals, res_data_errors, res_bin_centres = [], [], []
    
    for i in range(len(sim_values)):
        if sim_values[i] == 0.:
            continue
        
        residuals.append(data_values[i] / sim_values[i] - 1)
        res_data_errors.append(data_errors[i] / sim_values[i])
        res_bin_centres.append(bin_centres[i])
    
    res_sim_error_band = []
    
    for i in range(len(sim_values)):
        if sim_values[i] == 0.:
            res_sim_error_band.append(0.)
        else:
            res_sim_error_band.append(sim_errors[i] / sim_values[i])
    
    res_sim_error_band.append(res_sim_error_band[-1])
    res_sim_error_band = np.array(res_sim_error_band)
    
    
    # Plot the histograms
    fig = plt.figure()
    fig.patch.set_alpha(0.)
    gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[height_ratio, 1])
    axes_upper = fig.add_subplot(gs[0, 0])
    axes_lower = fig.add_subplot(gs[1, 0])
    
    axes_upper.errorbar(
        bin_centres, data_values, yerr=data_errors,
        color='black', marker='o', ls='none', label='Data'
    )
    axes_upper.hist(
        binning[:-1], bins=binning, weights=sim_values, histtype='stepfilled',
        color='#3399cc', label='Sim'
    )
    
    axes_lower.fill_between(
        binning, res_sim_error_band, -res_sim_error_band,
        step='post', color='0.75', lw=0
    )
    axes_lower.errorbar(
        res_bin_centres, residuals, yerr=res_data_errors,
        color='black', marker='o', ls='none'
    )
    
    # Remove tick labels on the x axis of the upper axes
    axes_upper.set_xticklabels([''] * len(axes_upper.get_xticklabels()))

    # Disable scientific notation for residuals
    axes_lower.ticklabel_format(axis='y', style='plain')
    
    axes_upper.set_xlim(binning[0], binning[-1])
    axes_lower.set_xlim(binning[0], binning[-1])
    axes_lower.set_ylim(-0.35, 0.37)
    axes_lower.grid(axis='y', color='black', ls='dotted')
    
    axes_lower.set_xlabel(x_label)
    axes_lower.set_ylabel(r'$\frac{\mathrm{Data} - \mathrm{MC}}{\mathrm{MC}}$')
    axes_upper.set_ylabel(y_label)
    
    # Manually set positions of labels of the y axes so that they are
    # aligned with respect to each other
    axes_upper.get_yaxis().set_label_coords(-0.1, 0.5)
    axes_lower.get_yaxis().set_label_coords(-0.1, 0.5)
    
    # Mark the under- and overflow bins
    axes_upper.text(
        0.995, 0.5, 'Overflow', transform=axes_upper.transAxes,
        ha='right', va='center', rotation='vertical', size='xx-small', color='gray'
    )
    
    if mark_underflow:
        axes_upper.text(
            0.005, 0.5, 'Underflow', transform=axes_upper.transAxes,
            ha='left', va='center', rotation='vertical', size='xx-small', color='gray'
        )
    
    
    # Build legend ensuring desired ordering of the entries
    legend_handles, legend_labels = axes_upper.get_legend_handles_labels()
    legend_handle_map = {}
    
    for i in range(len(legend_handles)):
        legend_handle_map[legend_labels[i]] = legend_handles[i]
    
    axes_upper.legend(
        [legend_handle_map['Data'], legend_handle_map['Sim']], ['Data', 'Simulation'],
        loc='upper right', frameon=False
    )
    
    axes_upper.text(1., 1., era_label, ha='right', va='bottom', transform=axes_upper.transAxes)
    
    return fig, axes_upper, axes_lower


def plot_balance(
    prof_pt_data, prof_pt_sim, prof_bal_data, prof_bal_sim,
    x_label=r'$p_\mathrm{T}^\mathrm{lead}$ [GeV]', y_label='', era_label='', height_ratio=2.,
    balance_range=(0.9, 1.)
):
    """Plot mean balance in data and simulation.
    
    Plot mean values of the balance observable in bins of pt of the
    leading jet, together with the ratio between data and simulation.
    In the upper panel x positions of markers are given by the profile
    of the leading jet's pt (and therefore might differ between data and
    simulation).  In the residuals panel mean pt in data is used as the
    position.  The overflow bin is plotted.
    """
    
    # Convert profiles to NumPy representations
    num_bins = prof_pt_data.GetNbinsX()
    data_x = np.zeros(num_bins + 1)
    data_y, data_yerr = np.zeros(num_bins + 1), np.zeros(num_bins + 1)
    
    for bin in range(1, num_bins + 2):
        if prof_pt_data.GetBinEntries(bin) > 0:
            data_x[bin - 1] = prof_pt_data.GetBinContent(bin)
        else:
            # This is an empty bin.  Its y value is also zero, so the
            # point will fall outside of the plotted range, but the x
            # coordinate must not be zero as this would a problem in
            # plotting with the log scale.
            data_x[bin - 1] = prof_pt_data.GetBinCenter(bin)
        
        data_y[bin - 1] = prof_bal_data.GetBinContent(bin)
        data_yerr[bin - 1] = prof_bal_data.GetBinError(bin)
    
    sim_x, sim_y, sim_yerr = np.zeros(num_bins + 1), np.zeros(num_bins + 1), np.zeros(num_bins + 1)
    
    for bin in range(1, num_bins + 2):
        sim_x[bin - 1] = prof_pt_sim.GetBinContent(bin)
        sim_y[bin - 1] = prof_bal_sim.GetBinContent(bin)
        sim_yerr[bin - 1] = prof_bal_sim.GetBinError(bin)
    
    sim_err_band_x = np.zeros(num_bins + 2)
    sim_err_band_y_low, sim_err_band_y_high = np.zeros(num_bins + 2), np.zeros(num_bins + 2)
    
    for bin in range(1, num_bins + 2):
        sim_err_band_x[bin - 1] = prof_pt_data.GetBinLowEdge(bin)
    
    # Since the last bin is the overflow bin, there is no natural upper
    # boundary for the error band.  Set ptMax = <pt> + |ptMin - <pt>|,
    # where <pt> is taken from simulation.
    sim_err_band_x[-1] = 2 * prof_pt_sim.GetBinContent(num_bins + 1) - \
        prof_pt_sim.GetBinLowEdge(num_bins + 1)
    
    sim_err_band_y_low = np.append(sim_y - sim_yerr, sim_y[-1] - sim_yerr[-1])
    sim_err_band_y_high = np.append(sim_y + sim_yerr, sim_y[-1] + sim_yerr[-1])
    
    
    # Compute residuals
    residuals = data_y / sim_y - np.ones(num_bins + 1)
    res_data_yerr = data_yerr / sim_y
    res_sim_err_band_y = np.append(sim_yerr / sim_y, sim_yerr[-1] / sim_y[-1])
    
    
    # Plot the graphs
    fig = plt.figure()
    fig.patch.set_alpha(0.)
    gs = mpl.gridspec.GridSpec(2, 1, hspace=0., height_ratios=[height_ratio, 1])
    axes_upper = fig.add_subplot(gs[0, 0])
    axes_lower = fig.add_subplot(gs[1, 0])
    
    axes_upper.set_xscale('log')
    axes_lower.set_xscale('log')
    
    axes_upper.errorbar(
        data_x, data_y, yerr=data_yerr,
        color='black', marker='o', ls='none', label='Data'
    )
    axes_upper.fill_between(
        sim_err_band_x, sim_err_band_y_low, sim_err_band_y_high, step='post',
        color='#3399cc', alpha=0.5, linewidth=0
    )
    axes_upper.plot(
        sim_x, sim_y,
        color='#3399cc', marker='o', mfc='none', ls='none', label='Sim'
    )
    
    axes_lower.fill_between(
        sim_err_band_x, res_sim_err_band_y, -res_sim_err_band_y, step='post', color='0.75'
    )
    axes_lower.errorbar(
        data_x, residuals, yerr=res_data_yerr,
        color='black', marker='o', ls='none'
    )
    
    # Remove tick labels in the upper axes
    axes_upper.set_xticklabels([''] * len(axes_upper.get_xticklabels()))
    
    # Provide a formatter for minor ticks so that thay get labelled.
    # Also set a formatter for major ticks in order to obtain a
    # consistent formatting (1000 instead of 10^3).
    axes_lower.xaxis.set_major_formatter(mpl.ticker.LogFormatter())
    axes_lower.xaxis.set_minor_formatter(mpl.ticker.LogFormatter(minor_thresholds=(2, 0.4)))

    # Disable scientific notation for residuals
    axes_lower.ticklabel_format(axis='y', style='plain')
    
    axes_upper.set_xlim(sim_err_band_x[0], sim_err_band_x[-1])
    axes_lower.set_xlim(sim_err_band_x[0], sim_err_band_x[-1])
    axes_upper.set_ylim(*balance_range)
    axes_lower.set_ylim(-0.02, 0.028)
    axes_lower.grid(axis='y', color='black', ls='dotted')
    
    axes_lower.set_xlabel(x_label)
    axes_lower.set_ylabel(r'$\frac{\mathrm{Data} - \mathrm{MC}}{\mathrm{MC}}$')
    axes_upper.set_ylabel(y_label)
    
    # Manually set positions of labels of the y axes so that they are
    # aligned with respect to each other
    axes_upper.get_yaxis().set_label_coords(-0.1, 0.5)
    axes_lower.get_yaxis().set_label_coords(-0.1, 0.5)
    
    # Mark the overflow bin
    axes_upper.text(
        0.995, 0.5, 'Overflow', transform=axes_upper.transAxes,
        ha='right', va='center', rotation='vertical', size='xx-small', color='gray'
    )
    
    
    # Build legend ensuring desired ordering of the entries
    legend_handles, legend_labels = axes_upper.get_legend_handles_labels()
    legend_handle_map = {}
    
    for i in range(len(legend_handles)):
        legend_handle_map[legend_labels[i]] = legend_handles[i]
    
    axes_upper.legend(
        [legend_handle_map['Data'], legend_handle_map['Sim']], ['Data', 'Simulation'],
        loc='upper right', frameon=True
    )
    
    axes_upper.text(1., 1., era_label, ha='right', va='bottom', transform=axes_upper.transAxes)
    
    return fig, axes_upper, axes_lower

