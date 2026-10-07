
import random
import argparse
from pathlib import Path

NUM_REQUESTS = 1000
MAX_ADDRESS_BYTES = 8 * 1024**3
REQUEST_ALIGNMENT_BYTES = 128
TIME_STEP_NS = 1.0
OUTPUT_FILENAME = "random_trace.txt"
RANDOM_SEED = 0


def generate_trace(
    output_path: Path,
    num_requests: int = NUM_REQUESTS,
    random_seed: int = RANDOM_SEED,
    time_step_ns: float = TIME_STEP_NS,
) -> bool:
    """Create a trace if it does not already exist.

    Returns True when a new trace is created and False when an existing file is
    preserved.
    """
    output_path = output_path.resolve()

    if output_path.exists():
        print(f"Trace already exists; leaving it unchanged: {output_path}")
        return False

    if num_requests <= 0 or time_step_ns <= 0:
        raise ValueError("Request count and time step must be positive")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    random_generator = random.Random(random_seed)

    with output_path.open("x", encoding="utf-8") as trace_file:
        for request_number in range(num_requests):
            timestamp_ns = request_number * time_step_ns
            operation = random_generator.choice(("READ", "WRITE"))
            physical_address = random_generator.randrange(
                0,
                MAX_ADDRESS_BYTES,
                REQUEST_ALIGNMENT_BYTES,
            )

            trace_file.write(
                f"{timestamp_ns:.1f} {operation} {physical_address:#x}\n"
            )

    print(f"Created {num_requests} requests in: {output_path}")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a synthetic trace")
    parser.add_argument("--output", type=Path, default=Path(OUTPUT_FILENAME))
    parser.add_argument("--requests", type=int, default=NUM_REQUESTS)
    parser.add_argument("--seed", type=int, default=RANDOM_SEED)
    parser.add_argument("--time-step-ns", type=float, default=TIME_STEP_NS)
    arguments = parser.parse_args()

    generate_trace(
        arguments.output,
        num_requests=arguments.requests,
        random_seed=arguments.seed,
        time_step_ns=arguments.time_step_ns,
    )


if __name__ == "__main__":
    main()
