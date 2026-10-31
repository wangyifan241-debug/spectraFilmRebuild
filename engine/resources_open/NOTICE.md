# Open material extensions

`profiles/open_kodak_trix_7266_033.json` is a **modified representation** of
Andrea Volpato's Kodak Tri-X Reversal Film 7266 profile, not the product's
Kodak Tri-X 400 negative. It remains CC BY-SA 4.0. The complete upstream
licence and preamble are in `SPEKTRAFILM_LICENSE.txt`; modifications are in
`CHANGELOG.txt`. Original arrays are preserved in `originals/kodak_trix.json`.

Source: https://github.com/andreavolpato/spektrafilm/blob/6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507/src/spektrafilm/data/profiles/kodak_trix.json

The source's one measured/digitized silver channel is represented in the
engine's existing three-channel schema. No core physical algorithm changes.
Only the source's one development condition is supported; grain/halation use
the existing engine defaults, not independent 7266 fits. This is a rendering
integration, not a claim of measured photographic equivalence.

`profiles/open_kodak_doublex_5222_65m_033.json` is a modified representation
of Andrea Volpato's Kodak Double-X Negative Film 5222 profile under the same
CC BY-SA 4.0 licence. Source (all five development conditions preserved in
`originals/kodak_doublex.json`):
https://github.com/andreavolpato/spektrafilm/blob/6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507/src/spektrafilm/data/profiles/kodak_doublex.json

The new version selects the supplied **6.5-minute condition**, including its
base density, curve, sublayers and model. It does not interpolate other times
or claim a new developer/temperature calibration. The same silver-channel
representation conversion applies. Grain/halation remain engine defaults,
not independently fitted Double-X parameters. Original SHA256:
`37c333046554151ac5538e6eeb1f2cadef919876c63d0dd861c547952874fac3`.

`profiles/open_kodak_2302_5m_033.json` is a modified representation of
Andrea Volpato's Kodak Print Film 2302, CC BY-SA 4.0. Source:
https://github.com/andreavolpato/spektrafilm/blob/6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507/src/spektrafilm/data/profiles/kodak_2302.json

All five supplied conditions remain in `originals/kodak_2302.json`. The
new version fixes the supplied **5-minute condition**, consistently for
base density, characteristic curves, sublayers and CDF. It retains
`support=film`, `stage=printing`: this is cine print film, not camera film
or paper. The Windows desktop sends an unfiltered neutral pack, NOT the
Ilford Multigrade grade-2 filter. The existing enlarger light/normalisation
and print/scanner defaults remain; this is not calibration of a particular
motion-picture optical printer. No new grain or scanner fits are claimed.
Original SHA256:
`3c04bb2e7701eae97eaaf14100cbf80c8f00dd07db3deda3f14b37ac060ca2be`.
