## Convolution
Convolution is a fundamental operation in probability theory, image recognition, neural networks, and even large integer multiplication. Since the naive $O(N^2)$ approach is computationally expensive, we can utilize parallelization to speed up processing. Alternatively, the **Fast Fourier Transform (FFT)** can be employed to lower the complexity to $O(N \log N)$.

## Parallelization Stategy
### Padding 


### Communication design

## Performance Analysis
The reference breaks down the complexity into computational (math work) and communication (data transfer work).



### 🧩 Computational Complexity (Per Process)
Since the grid is split among $P$ processes, the work is divided.

$N$: Input width <br>
$K$: Kernel width <br>
$P$: Number of processes <br>


Operations per input element: $O(K^2)$ <br>
Elements assigned to one process: $O(\frac{N(N+K)}{P})$ <br>

Total Computation:
$$O\left(\frac{N^2K^2 + NK^3}{P}\right)$$

<br>

### 🧩 Message Complexity (Communication)
The size of messages passed per process is proportional to the width of the input and the kernel: $O(NK)$. <br>

Total Message Complexity: Across all processes per forward pass, the complexity is $$O(PNK)$$

This is why more processors will get overhead.
## Reference
https://github.com/0xnirmal/Parallel-Convolution-MPI


https://www.youtube.com/watch?v=KuXjwB4LzSA