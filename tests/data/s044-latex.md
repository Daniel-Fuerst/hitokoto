## 7. Mathematical Notation ($\LaTeX$)

### 7.1 Inline Math
* Greek letters and subscripts: $\alpha_1, \beta_2, \gamma_3, \Omega, \omega, \phi, \varphi, \epsilon, \varepsilon$.
* Inline fractions and roots: $\frac{\sqrt{x^2 + y^2}}{z_0^2}$ and binomials $\binom{n}{k} = \frac{n!}{k!(n-k)!}$.
* Currency vs. Math ambiguity test: If an item costs $5.00 and another costs $10.00, does your parser accidentally turn `5.00 and another costs` into math mode? (Escaped: \$5.00 and \$10.00, vs real inline math $x = 5.00 + 10.00$).

### 7.2 Display Math Blocks
**Cauchy-Schwarz Inequality:**
$$
\left( \sum_{k=1}^n a_k b_k \right)^2 \leq \left( \sum_{k=1}^n a_k^2 \right) \left( \sum_{k=1}^n b_k^2 \right)
$$

**Maxwell's Equations (Aligned Environment):**
$$
\begin{aligned}
\nabla \cdot \mathbf{E} &= \frac{\rho}{\varepsilon_0} \\
\nabla \cdot \mathbf{B} &= 0 \\
\nabla \times \mathbf{E} &= -\frac{\partial \mathbf{B}}{\partial t} \\
\nabla \times \mathbf{B} &= \mu_0\left(\mathbf{J} + \varepsilon_0 \frac{\partial \mathbf{E}}{\partial t}\right)
\end{aligned}
$$

**Piecewise Function & Matrices:**
$$
f(x) = \begin{cases} 
x^2 \sin\left(\frac{1}{x}\right) & \text{if } x \neq 0 \\
0 & \text{if } x = 0 
\end{cases}
\quad \text{and} \quad
\mathbf{A} = \begin{pmatrix}
a_{11} & a_{12} & \cdots & a_{1n} \\
a_{21} & a_{22} & \cdots & a_{2n} \\
\vdots & \vdots & \ddots & \vdots \\
a_{m1} & a_{m2} & \cdots & a_{mn}
\end{pmatrix}
$$

**Chemical Equations:**
$$
\text{6 CO}_2 + \text{6 H}_2\text{O} \xrightarrow{h\nu} \text{C}_6\text{H}_{12}\text{O}_6 + \text{6 O}_2 \uparrow
$$