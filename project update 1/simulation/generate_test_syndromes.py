import stim
import pymatching
import numpy as np

# 1. Create a distance-3 rotated surface code circuit using Stim's built-in generator
distance = 3
rounds = 3
p = 0.01

circuit = stim.Circuit.generated(
    "surface_code:rotated_memory_x",
    distance=distance,
    rounds=rounds,
    after_clifford_depolarization=p,
    before_round_data_depolarization=p,
    after_reset_flip_probability=p,
    before_measure_flip_probability=p
)

# 2. Sample check matrices and error syndromes
num_shots = 1000
sampler = circuit.compile_detector_sampler()
detection_events, actual_observables = sampler.sample(num_shots, separate_observables=True)

print(f"Generated {num_shots} shots of syndrome data.")
print(f"Shape of detection events (syndromes): {detection_events.shape}")

# 3. Test a software baseline decoder (PyMatching) to verify correctness
dem = circuit.detector_error_model(decompose_errors=True)
matching = pymatching.Matching.from_detector_error_model(dem)

predicted_observables = matching.decode_batch(detection_events)
num_errors = np.sum([not np.array_equal(p, a) for p, a in zip(predicted_observables, actual_observables)])

print(f"Software Baseline Logical Error Rate: {num_errors / num_shots * 100}%")

# Save syndromes to a binary file to later feed into your FPGA host code
np.save("sample_syndromes.npy", detection_events)
print("Saved syndrome test vectors to sample_syndromes.npy")
