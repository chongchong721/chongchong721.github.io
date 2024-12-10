### Physics-based Rendering Project

![](/assets/images/pbr/drpssmlt.png)

- Markov chain Monte Carlo in primary sample space implementation.
- in the course project on PBR, I have implemented primary sample space Markov chain Monte Carlo[1] and delayed rejection MLT[2].
- The [report](/assets/docs/pbr/Final_project_report.pdf) is here
- The full code is not available since it contains course assignment source code which should not be publicly viewed. But part of it is here
  -  [random number generator(sampler) header](/assets/codes/pbr/sampler.h) 
  - [random number generator(sampler) source](/assets/codes/pbr/sampler.cpp)
  - [pss applied on path tracer](/assets/codes/pbr/path_tracer_pssmlt.h)
  - [drpss applied on path tracer](/assets/codes/pbr/path_tracer_drpssmlt.h)



[1]Csaba Kelemen, László Szirmay-Kalos, György Antal, and Ferenc Csonka. 2002. A Simple and Robust Mutation Strategy for the Metropolis Light Transport Algorithm. Computer Graphics Forum 21, 3 (2002), 531–540. https://doi.org/10.1111/1467-8659.t01-1-00703 arXiv:https://onlinelibrary.wiley.com/doi/pdf/10.1111/1467-8659.t01-1-00703

[2]Damien Rioux-Lavoie, Joey Litalien, Adrien Gruson, Toshiya Hachisuka, and Derek Nowrouzezahrai. 2020. Delayed Rejection Metropolis Light Transport. ACM Trans. Graph. 39, 3, Article 26 (may 2020), 14 pages. https://doi.org/10.1145/3388538