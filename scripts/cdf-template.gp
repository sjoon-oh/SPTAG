# Author: SukJoon Oh, sjoon@kaist.ac.kr

# Frequently used keywords
# httpointsize://hirophysics.com/gnuplot/gnuplot08.html

# Example:
# gnuplot -e "arg_filename_1='sequence-freqs.csv'; arg_export_name='export.png'" ./script/sequence-freq.gnuplot

# ────────────────────────────────── Source File ──────────────────────────────────────
# ─────────────────────────────────────────────────────┐
DATA_FNAME1                     = arg_filename_0
DATA_FNAME2                     = arg_filename_1
DATA_FNAME3                     = arg_filename_2
DATA_FNAME4                     = arg_filename_3
DATA_FNAME5                     = arg_filename_4
DATA_FNAME6                     = arg_filename_5

print sprintf("Using data file: %s", DATA_FNAME1)
print sprintf("Using data file: %s", DATA_FNAME2)
print sprintf("Using data file: %s", DATA_FNAME3)
print sprintf("Using data file: %s", DATA_FNAME4)
print sprintf("Using data file: %s", DATA_FNAME5)
print sprintf("Using data file: %s", DATA_FNAME6)


# ────────────────────────────────── Export File ──────────────────────────────────────
# ─────────────────────────────────────────────────────┐
EXPORT_NAME                     = arg_export_name
set output                      EXPORT_NAME


# 
# General settings
# ────────────────────────────────── Font Setting ─────────────────────────────────────
# ─────────────────────────────────────────────────────┐

FONT_GLOBAL                     ="Helvetica,28"     # Global font setting
FONT_TITLE                      ="Helvetica,28"     # Title font setting
FONT_XLABEL                     ="Helvetica,28"     # X-axis label font setting
FONT_YLABEL                     ="Helvetica,28"     # Y-axis label font setting
FONT_XTICS                      ="Helvetica,22"     # X-axis tics font setting
FONT_YTICS                      ="Helvetica,22"     # Y-axis tics font setting
FONT_KEY                        ="Helvetica,22"     # Key font setting

# ────────────────────────────────── Figure Specifics ─────────────────────────────────
# ─────────────────────────────────────────────────────┐
TITLE                           = ""

YLABEL                          = "CDF"
XLABEL                          = "Per-query Hit Ratio"

# ─────────────────────────────────────────────────────┐
set terminal                    pngcairo monochrome \
                                font FONT_GLOBAL \
                                size 750,550

set title TITLE                 font FONT_TITLE
unset title

set xlabel XLABEL               font FONT_XLABEL
set ylabel YLABEL               font FONT_YLABEL

set xtics                       nomirror
set ytics                       nomirror

# set xtics ("1" 1, "10" 10, "100" 100, "1K" 1000, "10K" 10000, "100K" 100000)
# set ytics ("1" 1, "10" 10, "100" 100, "1K" 1000, "10K" 10000, "100K" 100000)

set grid ytics

set key                         reverse \
                                samplen 2 width 0 height 0.5 \
                                left top \
                                maxrows 3 \
                                Left nobox font FONT_KEY

# ────────────────────────────────── Line Style ───────────────────────────────────────

set style line 1 linecolor rgb "red"     linetype 1 linewidth 1.5 pointtype 1 pointsize 1.5 pi -1  ## +
set style line 2 linecolor rgb "blue"    linetype 2 linewidth 1.5 pointtype 2 pointsize 1.5 pi -1  ## x
set style line 3 linecolor rgb "#00CC00" linetype 1 linewidth 1.5 pointtype 3 pointsize 1.5 pi -1  ## *
set style line 4 linecolor rgb "#7F171F" linetype 4 linewidth 1.5 pointtype 4 pointsize 1.5 pi -1  ## box
set style line 5 linecolor rgb "#FFD800" linetype 3 linewidth 1.5 pointtype 5 pointsize 1.5 pi -1  ## solid box
set style line 6 linecolor rgb "#000078" linetype 6 linewidth 1.5 pointtype 6 pointsize 1.5 pi -1  ## circle
set style line 7 linecolor rgb "#732C7B" linetype 7 linewidth 1.5 pointtype 7 pointsize 1.5 pi -1
set style line 8 linecolor rgb "black"   linetype 8 linewidth 1.5 pointtype 8 pointsize 1.5 pi -1  ## triangle

# set style line 1 linecolor rgb "black"    linetype 1 linewidth 2 pointtype 1 pointsize 1.5 pi -1  ## +
# set style line 2 linecolor rgb "black"    linetype 2 linewidth 2 pointtype 2 pointsize 1.5 pi -1  ## x
# set style line 3 linecolor rgb "black"    linetype 1 linewidth 2 pointtype 3 pointsize 1.5 pi -1  ## *
# set style line 4 linecolor rgb "black"    linetype 4 linewidth 2 pointtype 4 pointsize 1.5 pi -1  ## box
# set style line 5 linecolor rgb "black"    linetype 3 linewidth 2 pointtype 5 pointsize 1.5 pi -1  ## solid box
# set style line 6 linecolor rgb "black"    linetype 6 linewidth 2 pointtype 6 pointsize 1.5 pi -1  ## circle
# set style line 7 linecolor rgb "black"    linetype 7 linewidth 2 pointtype 7 pointsize 1.5 pi -1
# set style line 8 linecolor rgb "black"    linetype 8 linewidth 2 pointtype 8 pointsize 1.5 pi -1  ## triangle


# ────────────────────────────────── Plotting ──────────────────────────────────────────

# set logscale y
# set logscale x

# set style                       data histogram
# set style                       histogram cluster gap 1

# set style                       box

# set style                       fill solid border -1



set ytics                       0.1
set xtics                       

set border 3                    # Remove the top and right border

plot                            DATA_FNAME1 \
                                using 1:2 \
                                title "16GiB" \
                                with lines ls 1, \
                                \
                                DATA_FNAME2 \
                                using 1:2 \
                                title "1GiB" \
                                with lines ls 2, \
                                \
                                DATA_FNAME3 \
                                using 1:2 \
                                title "2GiB" \
                                with lines ls 3, \
                                \
                                DATA_FNAME4 \
                                using 1:2 \
                                title "32GiB" \
                                with lines ls 4, \
                                \
                                DATA_FNAME5 \
                                using 1:2 \
                                title "4GiB" \
                                with lines ls 5, \
                                \
                                DATA_FNAME6 \
                                using 1:2 \
                                title "8GiB" \
                                with lines ls 6, \

