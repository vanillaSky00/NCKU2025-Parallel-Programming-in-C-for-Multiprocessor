### 1. Key: $f(x) = \lfloor \frac{n}{x} \rfloor$ is **monotonically decreasing**.
Instead of iterating $1 \to n$, we iterate $1 \to \sqrt{n}$  by exploiting symmetry.

### 2. The Boundary: $\sqrt{n}$

  * **Factor Pairs:** Factors come in pairs $(a, b)$ where $a \cdot b \approx n$.
  * **The Split:** One factor is always $\le \sqrt{n}$, and the other is $\ge \sqrt{n}$.
  * **Implication:** Once $x > \sqrt{n}$, the result $\lfloor n/x \rfloor$ becomes smaller than $\sqrt{n}$ and repeats frequently.

### **3. The Strategy: Steep vs. Flat**

| Range | Curve Shape | Behavior | Iteration Strategy |
| :--- | :--- | :--- | :--- |
| **$1 \le x \le \sqrt{n}$** | **Steep** | Outputs are unique and change rapidly.<br>*(e.g., $100/1=100, 100/2=50$)* | **Iterate Input ($x$):**<br>Standard summation. |
| **$\sqrt{n} < x \le n$** | **Flat** | Outputs are small and repeat often.<br>*(e.g., $100/51 \dots 100/99$ all equal $1$)* | **Iterate Output ($y$):**<br>Calculate range $[l, r]$ and multiply by count. |

 Switch techniques at $\sqrt{n}$ because it is the equilibrium point where the curve transitions from unique values (steep) to repeated values (flat).

 ### Implement pitfall
 value cast:
 ```
 // Pitfall: sqrt returns double (e.g., 3.9999 -> 3)
int sqrt_n = floor(sqrt(n));
 ```
 loop idx:
 ```
 // WRONG (Misses values)
// int start_i = sqrt_n - 1; 
// Example N=12: Starts at 2. Misses 3.

// CORRECT (Math-derived)
int start_i = n / (sqrt_n + 1); 
// Example N=12: 12 / 4 = 3. Catches 3.
 ```