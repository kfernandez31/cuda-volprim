class GaussianKernel(Kernel):
    def __init__(self, props):
        super().__init__(props)

    def eval(self,
             p: mi.Point3f,
             ellipsoid: Ellipsoid,
             active: mi.Bool) -> mi.Float:
        p = ellipsoid.rot.T * (p - ellipsoid.center)
        s = ellipsoid.scale
        p = p / s
        return dr.exp(-0.5 * dr.sum(p * p))

    def pdf(self,
            p: mi.Point3f,
            ellipsoid: Ellipsoid,
            active: mi.Bool) -> mi.Float:
        p = ellipsoid.rot.T * (p - ellipsoid.center)
        s = ellipsoid.scale
        p = p / s

        denom = dr.sqrt(8.0 * dr.pi * dr.pi * dr.pi) * dr.prod(s)
        pdf = dr.exp(-0.5 * dr.sum(p * p)) / denom
        return dr.select(active, pdf, 0.0)

    def inv_cdf(self,
                ray: mi.Ray3f,
                ellipsoid: Ellipsoid,
                sigmat: mi.Float,
                chi: mi.Float,
                active: mi.Bool) -> mi.Float:
        s = ellipsoid.scale
        w = ellipsoid.rot.T * ray.d / s
        p = ellipsoid.rot.T * (ray.o - ellipsoid.center) / s
        w_norm = dr.norm(w)
        w /= w_norm

        B = dr.dot(w, p)
        C = dr.dot(p, p)

        K = sigmat * dr.rcp(w_norm) * dr.exp(-0.5 * (C - B * B)) * dr.rcp(dr.sqrt(4.0 * (dr.pi * dr.pi)) * dr.prod(s))
        erfinv_arg = dr.erf(B / dr.sqrt(2.0)) + 2.0 * chi / K
        t_s = dr.sqrt(2.0) * dr.erfinv(erfinv_arg) - B
        t_s = dr.select(dr.isnan(t_s), dr.inf, t_s)  # Handle NaNs arising from intersection points in the infinite (dr.abs(erfinv_arg)>1)
        # TODO: would be interesting that erfinv returns -inf or inf instead of nans, could we change this behavior
        
        return dr.select(active, t_s, 0.0)

    def density_integral(self,
                         ray: mi.Ray3f,
                         ellipsoid: Ellipsoid,
                         tmin: mi.Float,
                         tmax: mi.Float,
                         active: mi.Bool) -> mi.Float:
        if self.full_range or (tmin is None and tmax is None):
            w = ellipsoid.rot.T * ray.d
            p = ellipsoid.rot.T * (ray.o - ellipsoid.center)
            s = ellipsoid.scale
            w /= s
            w_norm = dr.norm(w)
            w /= w_norm
            p /= s

            B = dr.dot(w, p)
            C = dr.dot(p, p)
            density = dr.sqrt(2.0 * dr.pi) * dr.rcp(w_norm) * dr.exp(-0.5 * (C - B * B))
        else:
            active = mi.Bool(active) & (tmin < tmax) & (tmax > 0.0) # Catching rare edge-cases and avoiding (NaNs)
            s = ellipsoid.scale
            w = ellipsoid.rot.T * ray.d / s
            p = ellipsoid.rot.T * (ray.o - ellipsoid.center) / s
            w_norm = dr.norm(w)
            w /= w_norm
            t_sim = 0.5 * (tmax - tmin)
            t_sim *= w_norm
            p = p + t_sim * w

            B = dr.dot(w, p)
            C = dr.dot(p, p)
            erf = dr.erf(t_sim * dr.sqrt(0.5))
            density = dr.rcp(w_norm) * dr.exp(-0.5 * (C - B * B)) * erf * dr.sqrt(2.0 * dr.pi)
        
        if self.normalized:
            density *= self.normalization_factor(s)

        density = dr.maximum(density, 0.0)
        density[~active] = 0.0
        density[~dr.isfinite(density)] = 0.0

        return density
    
    def hierarchical_density_integral(self,
                                      ray: mi.Ray3f,
                                      ellipsoid: Ellipsoid,
                                      tmin: mi.Float,
                                      tmax: mi.Float,
                                      active: mi.Bool) -> mi.Float:
        raise Exception('Regular Gaussian Kernel has no hierarchy')
                                      

    def normalization_factor(self, s: mi.Vector3f) -> mi.Float:
        return dr.rcp(dr.sqrt(8.0 * (dr.pi * dr.pi * dr.pi)) * dr.prod(s))