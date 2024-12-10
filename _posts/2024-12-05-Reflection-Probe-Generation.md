The project is based on the paper 'Fast Filtering of Reflection Probe'

The idea is simple: When filtering the environment map using a distribution of normals(*NDF*) kernel, which is


$$
\int_\Omega L(l)D(h)dl
$$
it is possible to find optimal directions rather than using Monte Carlo importance sampling, so that, given same amount of sample budgets(or same amount of execution time budget), the filtered result will be closer to the ground truth result computed by numerical integration.

The optimization process is try to find $N$ samples that could best reconstruct a specific GGX NDF kernel on a cubemap. And the optimization should be run on many normal directions on a cubemap.

The original paper proposed the way to do this for GGX material in radial symmetric case($n=v$), using a pre-optimized polynomial coefficient table to compute those directions. This project aims at

- Re-implement the optimization pipeline, which is not published.
- Extend this to view-dependent case(i.e. without the assumption of $n=v$)
- Extend this to other real-life materials.





#### Constraints

- Given this application is in real-time graphics context, the direction computation must not take a lot of time(e.g. we can not put things into a neural network and wait for the output?). This might depend on the use case. If the environment map update is not so frequent, and one can tolerate the trade off between accuracy and time, a more complex fitting function can be used.
- The radial symmetry assumption of the split sum approximation reduce the problem dimension. While this assumption is OK for many usecase(if it is a sphere), it can not reproduce the stretched, anisotropic visual effect.

#### Current Results

*This post has no images at this time/code([Github Repo](https://github.com/chongchong721/ggx_filtering) ) is not tidy*. 

##### Reimplementation

- There seems to be some coding bugs in the reference HLSL code provided by the original author. It is related to a Jacobian. The code uses the inverse of it while the paper says otherwise.
- An optimization pipeline using Pytorch(Adam/L-BFGS) is implemented. It supports generating coefficient table for any GGX roughness. Though it is written in Python, with CUDA support it runs pretty fast.

##### View-dependent case

- Adding view-dependent case introduces a lot of trouble
  - More parameters
    - Given the material is isotropic, we can focus only on $\theta_h$, however, due to we are operating on a cubemap, not a sphere, the distortion may require more complex parameterization.
  - Invalid samples that are below horizon
    - If we treat $\theta_h$ as a variable in the polynomial. For cases where $\theta_h$ is are close to grazing angles, we might have a lot of samples that are below the horizon. Eventually, grazing angles are the part that we want to make it better so this is not acceptable.
      - Adding a regularization term of $ReLU(-\langle n,l \rangle)$ during the optimization will make most samples valid. However, this introduces visible bias. In a filtered map, it looks like the map is rotated. For other cases that do not require clipping the horizon, the sample distribution is regularized to the same shape as grazing angles(imaging half of the GGX kernel is clipped in an extreme case)
  - Different shapes of GGX kernel for different $\theta_h$
    - The shape difference of a GGX kernel at grazing angles and not at grazing angles is huge. It seems like the polynomial fitting method can not capture them at the same time
- How this might be solved
  - Optimize different coefficient tables for different $\theta_h$, when there is a $\theta_h$ in between, we interpolate.

##### Other Materials

- The original method is effective in isotropic radial symmetric case. For other isotropic materials, we will need the NDF to do this, since all the theory of split sum approximation is based on microfacet theory, 
- There is existing way to extract tabulated NDF from retro-reflective measurements. I already re-implemented this. We can test it on some MERL materials.