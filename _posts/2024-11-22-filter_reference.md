#### A Note on Prefiltered GGX Environment Map

The famous first term of Epic split sum approximation:
$$
\frac{1}{N}\sum_{k=1}^N L(l_k)
$$


They use this form because they are doing importance sampling and then Monte Carlo.

When you importance sampling the lighting direction according to GGX NDF, the actual pdf of this light direction is 
$$
pdf(l) = pdf(\omega_m)*\frac{1}{4\cos\langle v,\omega_m \rangle}
$$
where $\frac{1}{4cos}$ is the Jacobian from half vector to light/view direction

And, note that, the way to importance sample GGX NDF(*not actually GGX NDF*) is
$$
\theta_m = \arctan{(\frac{\alpha \sqrt{\xi_1}}{\sqrt{1-\xi_1}})}
$$

$$
\phi_m = 2\pi \xi_2
$$

After taking the Jacobian from spherical coordinate to solid angle:$d\omega = \sin{\theta}d\theta d\phi$

We will get:
$$
pdf(\omega_m) = D(\omega_m)\langle \omega_m,n \rangle
$$
For the above part, refer to https://schuttejoe.github.io/post/ggximportancesamplingpart1/

This means, when you are sampling $\theta$ and $\phi$ using the above formula provided by the original GGX paper, you are actually importance sampling $D(\omega_m)\langle \omega_m,n \rangle$, not $D(\omega_m)$, some literature is not very clear on this.

So, actually we are computing the integral:
$$
\int L(l)D(\omega_m)\langle \omega_m,n \rangle \frac{1}{4\cos\langle v,\omega_m \rangle}dl
$$
So, if you are doing numerical integration , be sure to directly compute this term and add it up. And don't forget any Jacobian involved. Or, if you are doing something differently than Epic, be sure to derive a different prefiltering formula.



Then, Epic states that, they've found adding another term $\langle l,n \rangle$ can help the result to be better. This is purely added empirically, there is no math behind this. In the end, all of these are just approximations, so probably we should stick to what gives a better result.