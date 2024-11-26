### Computational Photography Final Project

##### BRDF Reconstruction using MERL database

- In this project, I have implemented two BRDF reconstruction techniques. One is using PCA to find a few optimal sample directions that could best capture the BRDF[1]. Another one is using two camera direction/light direction pair and extract results from these two 2D images[2].
- I applied these techniques to some isotropic materials.
-  I've done some analysis and point out the problems and possible improvements.
  - Grazing angles
    - This method sometimes ignore the darker color of ground truth
      MERL material at grazing angles.
  - A faster but better error metric
    - In optimizing the directions, I can not run the error metric locally
      with a laptop of 32GB memory. It would take more than 10 hours to
      optimize 10 point direction.
  - Putting more materials into the BRDF space
    - If we have more materials other than MERL, we can put all of them
      into the material space. Thus the reconstruction could be more
      robust as there is more liklihood for a new material to lie in the
      subspace of all known material.
  - Low RMSE Wrong Rendered Image
    - Reconstruction having the lowest RMSE is the most different from
      the ground truth during the rendering comparison.
- The report could be found here [slides](/assets/docs/cp/BRDF_reconstruction_using_MERL_database.pptx),[pdf](/assets/docs/cp/Final_project_report.pdf),[video](/assets/videos/cp/video.mp4)
- The source code is here [Github](https://github.com/chongchong721/cp_final_project_brdf)



[1]Jannik Boll Nielsen, Henrik Wann Jensen, and Ravi Ramamoorthi. 2015. On Optimal, Minimal BRDF Sampling for Reflectance Acquisition. ACM Trans. Graph. 34, 6, Article 186 (nov 2015), 11 pages. https://doi.org/10.1145/2816795.2818085

[2]Zexiang Xu, Jannik Boll Nielsen, Jiyang Yu, Henrik Wann Jensen, and Ravi Ramamoorthi. 2016. Minimal BRDF Sampling for Two-Shot near-Field Reflectance Acquisition. ACM Trans. Graph. 35, 6, Article 188 (dec 2016), 12 pages. https://doi.org/10.1145/2980179.2982396