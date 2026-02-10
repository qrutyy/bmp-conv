# Filters Reference

This document lists all convolution filters supported by **bmp-conv**.

---

## Available Filters

| Code | Name                | Description                 |
| ---- | ------------------- | --------------------------- |
| `bb` | Blur                | Simple averaging blur       |
| `bo` | Box Blur            | Uniform box filter          |
| `mb` | Motion Blur         | Directional blur            |
| `gb` | Gaussian Blur       | Standard Gaussian smoothing |
| `gg` | Gaussian Blur (Big) | Larger kernel Gaussian blur |
| `sh` | Sharpen             | Enhances edges              |
| `em` | Emboss              | Embossing effect            |
| `mm` | Median              | Median noise reduction      |
| `mg` | Median Gaussian     | Hybrid median + Gaussian    |
| `co` | Convolution         | Generic convolution kernel  |

---

## Filter Categories

### Linear Filters

* Blur
* Box Blur
* Gaussian Blur
* Sharpen
* Emboss

### Non-Linear Filters

* Median
* Median Gaussian

---

## Notes

* Kernel sizes are fixed per filter implementation
* Edge handling uses clamping
* Some filters are compute-intensive and scale better with `by_grid`

---

## Performance Considerations

* Median-based filters have higher computational cost
* Gaussian filters benefit from block-based partitioning
* Queue-mode improves throughput for multiple images
