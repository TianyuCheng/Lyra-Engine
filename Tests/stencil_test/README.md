# Stencil Test

## Description
This test will first draw a single triangle into the stencil buffer.
Then it will draw the two triangle overlapping geometry using the stencil buffer.
If stencil test is working as expected, we should see only the region where the
first triangle is drawn will be covered by two colors.

## Reference
![Reference](reference.png "Reference")
