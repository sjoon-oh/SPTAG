# Author: SukJoon Oh, sjoon@kaist.ac.kr

# Frequently used keywords
# httpointsize://hirophysics.com/gnuplot/gnuplot08.html

# Example:
# gnuplot -e "arg_filename_1='sequence-freqs.csv'; arg_export_name='export.png'" ./script/sequence-freq.gnuplot

# ────────────────────────────────── Source File ──────────────────────────────────────
# ─────────────────────────────────────────────────────┐
DATA_FNAME1                     = arg_filename_0

TOTAL_RECORDS                   = system("wc -l " . DATA_FNAME1 . " | awk '{print $1}'")

print sprintf("Using data file: %s", DATA_FNAME1)
print sprintf("Total records: %s", TOTAL_RECORDS)


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

YLABEL                          = "Occurrances (Log Scale)"
XLABEL                          = "Posting List ID (Log Scale)"

# ─────────────────────────────────────────────────────┐
set terminal                    pngcairo monochrome \
                                font FONT_GLOBAL \
                                size 850,650

set title TITLE                 font FONT_TITLE
unset title

set xlabel XLABEL               font FONT_XLABEL
set ylabel YLABEL               font FONT_YLABEL

set xtics                       nomirror
set ytics                       nomirror

set grid ytics

set key                         reverse \
                                samplen 2 width -6 height 0.5 \
                                right top \
                                maxrows 2 \
                                Right nobox font FONT_KEY

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

set logscale y
set logscale x

set xrange                      [1:10000000]
set yrange                      [1:10000000]    

# set style                       data histogram

set xtics                       ("1" 1,             \
                                    "10" 10,        \
                                    "100" 100,      \
                                    "1K" 1000,      \
                                    "10K" 10000,    \
                                    "100K" 100000,  \
                                    "1M" 1000000,   \
                                    "10M" 10000000  \
                                    )

set ytics                       ("1" 1,             \
                                    "10" 10,        \
                                    "100" 100,      \
                                    "1K" 1000,      \
                                    "10K" 10000,    \
                                    "100K" 100000,  \
                                    "1M" 1000000,   \
                                    "10M" 10000000  \
                                    )
# set ytics ("1" 1, "10" 10, "100" 100, "1K" 1000, "10K" 10000, "100K" 100000)

# Zipfian Distribution
zipf(x) = TOTAL_RECORDS / x**0.99

# set ytics                       0.1
# set xtics                       

set border 3                    # Remove the top and right border

plot                            DATA_FNAME1 \
                                using 2 \
                                title "Posting Lists" \
                                with points, \
                                \
                                zipf(x) \
                                title "Power Law (α=0.99)" \
                                with lines \
                                lt 2 linecolor rgb "red"