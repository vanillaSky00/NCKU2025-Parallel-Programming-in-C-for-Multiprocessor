## Convolution
Convolution is a fundamental operation in probability theory, image recognition, neural networks, and even large integer multiplication. Since the naive $O(N^2)$ approach is computationally expensive, we can utilize parallelization to speed up processing. Alternatively, the **Fast Fourier Transform (FFT)** can be employed to lower the complexity to $O(N \log N)$.

## Setup
```
chmod u+x judge.py
python3 judge.py
```

## Optimization Analysis
#### problem constrains
Input matrix: $n \times n, \ n \le 200$
 <br> Kernel: $3 \times 3$ 
 <br> Times of operation $t \le 10^4$ 

 **Worst case:** 
<br> Matrix Size: $200 \times 200 = 40,000$ elements.<br> Kernel Operations: For each element, a $3 \times 3$ convolution does 9 multiplications and 9 additions. Let's say roughly 20 operations per element.
<br> Iterations: $10,000$ passes.

<br> Total Operations:$$40,000 \text{ elements} \times 20 \text{ ops/element} \times 10,000 \text{ iterations} \approx 8 \times 10^9 \text{ operations (8 Billion)}$$

And A modern CPU core typically handles $2-4$ billion operations per second (single-threaded).



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


## MPI
The "v" in MPI_Gatherv stands for Variable. Unlike MPI_Gather (which assumes everyone sends the exact same amount), MPI_Gatherv allows processes to send different amounts of data. This is crucial because your last process might have a different number of rows (handling the remainder of $N / P$).

## Reference

https://hackmd.io/@HankWu0425/Bonus1

https://github.com/0xnirmal/Parallel-Convolution-MPI

https://www.youtube.com/watch?v=KuXjwB4LzSA