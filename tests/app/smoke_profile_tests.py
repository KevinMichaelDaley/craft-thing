"""Direct executable defaults must match the accepted displayed smoke launcher."""
import os
import subprocess

base = {k: v for k, v in os.environ.items() if not k.startswith('DC_GAS_')}
cases = [{}, {'DC_GAS_COARSE': '0', 'DC_GAS_PRESSURE_SWEEPS': '24', 'DC_GAS_LOCAL_STAGES': '0'},
         {'DC_GAS_LOCAL_STAGES': '0', 'DC_GAS_EXPLICIT_RATE_CACHE': '1'}]
failed = 0
for overrides in cases:
    env = dict(base, **overrides)
    expected = subprocess.check_output(['bash', '-c',
        'source tools/smoke_profile; for key in ${!DC_GAS_@}; do printf "%s=%s\\n" "$key" "${!key}"; done'],
        env=env, text=True)
    result = subprocess.run(['./build/smoke_profile_probe'], env=env, text=True, capture_output=True)
    if result.returncode or sorted(result.stdout.splitlines()) != sorted(expected.splitlines()):
        print('FAIL smoke defaults differ from production launcher for', overrides)
        failed += 1
print(f'{len(cases)-failed} passed, {failed} failed')
raise SystemExit(bool(failed))
