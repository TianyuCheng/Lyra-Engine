# Post-Process Depth of Field Visual Test

Validates the physical Depth of Field pass (`DepthOfFieldPass`):
- Circle of Confusion (CoC) derivation based on camera sensor height, focal length, aperture f-number, and focal plane distance
- 32-sample golden spiral circular bokeh disc filter with bilateral depth-aware weighting
- Verification of razor-sharp in-focus plane and smooth bokeh disc blur on foreground and background geometry
