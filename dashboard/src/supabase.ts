import { createClient } from "@supabase/supabase-js";
const url = import.meta.env.VITE_SUPABASE_URL?.trim();
const key = import.meta.env.VITE_SUPABASE_PUBLISHABLE_KEY?.trim();
export const configured = Boolean(url && key && !url.includes("YOUR_PROJECT_REF"));
export const supabase = configured ? createClient(url, key, { auth: { persistSession: false } }) : null;

