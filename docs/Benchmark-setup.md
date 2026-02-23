# Benchmark-system setup

This project includes Python scripts in the `tests/` directory for benchmarking analysis and plotting results. To run these scripts, you need Python 3.x and several libraries.

### Setting up the Python Environment

It is recommended to use a virtual environment to manage Python dependencies.

1.  **Navigate to the project root directory:**
    ```bash
    cd bmp-conv
    ```

2.  **Create a virtual environment (e.g., named `venv_py`):**
    ```bash
    python3 -m venv venv_py
    ```

3.  **Activate the virtual environment:**
    **Linux/macOS:**
    ```bash
    source venv_py/bin/activate
    ```

4.  **Install the required Python packages:**
    ```bash
    pip install -r requirements-dev.txt
    ```

#### Optional:

1. If you are using **convert** for file generation - set the upper limit of the disk usage:
    ```xml
    <policy domain="resource" name="disk" value="8GiB"/>
    ```
    and 
    ```xml
    <policy domain="resource" name="width" value="128KP"/>
    <policy domain="resource" name="height" value="128KP"/>
    ```
