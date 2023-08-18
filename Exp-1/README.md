# (Exp #1) Query-driven telemetry

You can run the simulation on any host.

## Requirement

- `libcrcutil`:
  ```sh
  sudo apt install libcrcutil-dev
  ```

## Build

```sh
# Make sure you are at the ${OmniWindow_DIR}/Exp-1/ directory.
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
cd ..
```

## Experiement

1. Run the experiment:
   ```sh
   # Make sure you are at the ${OmniWindow_DIR}/Exp-1/ directory.
   . run.sh
   ```
   and wait for all jobs to finish (you can see their status with command `jobs`, or wait for all jobs with command `wait`).

2. Merge the results in a table:
   ```sh
   # Make sure you are at the ${OmniWindow_DIR}/Exp-1/ directory.
   python3 merge-result.py result
   ```
   Then the results are in `${OmniWindow_DIR}/Exp-1/result/recall.csv` and `${OmniWindow_DIR}/Exp-1/result/precision.csv`.
