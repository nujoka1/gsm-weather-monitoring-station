# Deployment

## Supabase gate

Create a new project named `weather-monitoring-station`. Before creation, list organizations and current regions, request the platform's cost quote, and obtain explicit approval for organization, region, and cost. Never reuse an unrelated project.

After login, use the current CLI help rather than copying stale flags. Link the new project, apply the migration, set `WEATHER_STATION_API_KEY` from an ignored local environment file, and deploy `ingest-weather`. `verify_jwt = false` is deliberate because SIM800L authenticates with a station-specific header instead of a Supabase user JWT. The function itself rejects missing/wrong keys and uses a server-only secret key for database insertion.

Current hosted project: `gwisrdrvhwpawahlqajm` in London (`eu-west-2`). The schema and function are deployed. Generate the station key locally with `openssl rand -hex 32`; place the value in the ignored firmware `secrets.h` and set the identical value as the function secret named `WEATHER_STATION_API_KEY`. Do not paste it into documentation, Git, URLs, screenshots, or chat.

Run database security and performance advisors after applying the migration. Confirm RLS is enabled, anonymous `SELECT` works for active stations, and anonymous writes fail.

## Dashboard

Only configure `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY`. Never put a Supabase secret/service-role key in any `VITE_` variable. Build with `npm run build` from `dashboard`; deploy the generated `dist` directory to a static HTTPS host.

## Device endpoint tests

- no `x-station-key` -> `401`
- wrong key -> `401`
- malformed JSON -> `400`
- physically impossible measurement -> `422`
- non-POST method -> `405`
- valid reading -> `201`, followed by a read-only database check proving exactly one inserted row
