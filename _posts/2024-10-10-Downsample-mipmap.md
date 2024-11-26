### Downsample A Cubemap Mipmap

In the Activision paper of fast filtering of GGX reflection probe, the author proposed a smoother downsample routine. However, it seems like the code the author provided is not doing what they discussed in their paper.

A B-spline kernel is used for downsampling. And Jacobian terms are used to account for the distortion from cubemap to sphere mapping

>Quadratic b-splines are smooth, but the projection of a constant function over a sphere onto a cubemap is not smooth between faces. To get a basis that better approximates a constant over a sphere, we weight each of the samples in the b-spline recurrence by the Jacobian, J(x, y, z). Weighting samples by the Jacobian has the desired effect of giving less weight to corners and edges, and produces functions that are smooth everywhere except for at edges.

This means, for a value $v$ at a cubemap location $(x,y,z)$, we weight it by
$$
v * \frac{1}{(x^2 + y^2 + z^2)^{\frac{3}{2}}}
$$
By doing this, less weight is given to the locations near corners and edges.

However, the code seems to be doing the contrary:

```c++
float calcWeight( float u, float v )
{
	float val = u*u + v*v + 1;
	return val*sqrt( val );
}
float weights[4];
weights[0] = calcWeight( u0, v0 );
weights[1] = calcWeight( u1, v0 );
weights[2] = calcWeight( u0, v1 );
weights[3] = calcWeight( u1, v1 );

const float wsum = 0.5f / ( weights[0] + weights[1] + weights[2] + weights[3] );
[unroll]
for ( int i = 0; i < 4; i++ )
    weights[i] = weights[i] * wsum + .125f;

get_dir_0( dir, u0, v0 );
color = tex_hi_res.SampleLevel( bilinear, dir, 0 ) * weights[0];

get_dir_0( dir, u1, v0 );
color += tex_hi_res.SampleLevel( bilinear, dir, 0 ) * weights[1];

get_dir_0( dir, u0, v1 );
color += tex_hi_res.SampleLevel( bilinear, dir, 0 ) * weights[2];

get_dir_0( dir, u1, v1 );
color += tex_hi_res.SampleLevel( bilinear, dir, 0 ) * weights[3];
break;
```

So, this is weighting it by $\frac{1}{J(x,y,z)}$, it gives larger weight at corners and edges. 