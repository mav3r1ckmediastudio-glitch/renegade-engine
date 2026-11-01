"""Fit a monotonic sky-occupancy to declared admission domain from GPU captures."""
from pathlib import Path
import argparse,json,statistics
p=argparse.ArgumentParser()
p.add_argument("folder")
p.add_argument("legacy_statistic",nargs="?",choices=("upper","mean"))
p.add_argument("--cloud-base",type=float,default=1500)
p.add_argument("--cloud-depth",type=float,default=5000)
p.add_argument("--admission-domain",choices=("planar","whole_body_rank"),default="planar")
args=p.parse_args()
root=Path(args.folder)
rows=json.loads((root/"opacity-metrics.json").read_text(encoding="utf-8"))
groups={}
for row in rows:groups.setdefault(float(row["coverage"]),[]).append(row["sky_fraction_alpha05"])
upper=args.legacy_statistic=="upper"
statistic=max if upper else statistics.mean
curve=[(0.0,0.0)]+[(statistic(v),k) for k,v in sorted(groups.items())]+[(1.0,1.0)]
assert all(a[0]<=b[0]+1e-7 for a,b in zip(curve,curve[1:])),curve
def inverse(target):
    if target<=0:return 0.0
    if target>=1:return 1.0
    for (x0,y0),(x1,y1) in zip(curve,curve[1:]):
        if x1>x0 and target<=x1:
            return y0+(target-x0)*(y1-y0)/(x1-x0)
    return 1.0
table=[inverse(i/16) for i in range(17)]
domain_key="body_rank_probability" if args.admission_domain=="whole_body_rank" else "planar_admission"
result=dict(statistic="upper_view_occupancy" if upper else "mean_sky_occupancy",
    admission_domain=args.admission_domain,
    measured_curve=[dict(sky_occupancy=x,**{domain_key:y}) for x,y in curve],
    group_ranges=[dict(**{domain_key:k},minimum=min(v),maximum=max(v),
        mean=statistics.mean(v),views=len(v)) for k,v in sorted(groups.items())],
    inverse_lut17=table,
    declared_fixture=dict(formation="Cumulus",cloud_base=args.cloud_base,
        cloud_depth=args.cloud_depth,positions=3,pitches=[35,70],
        local_weather_ceiling=None if args.admission_domain=="whole_body_rank" else .80),
    note="Fixture dimensions declared by runner. Fit is not independent validation. Hold out other positions, geometry, APIs and angles.")
(root/"admission-calibration.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
print(json.dumps(result,indent=2))
