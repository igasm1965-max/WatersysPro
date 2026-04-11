-- Migration 002: Add telemetry retention and performance indexes
-- Run manually or via migration runner

-- Partition-ready: add index for time-based cleanup
CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(ts);

-- Retention policy: delete telemetry older than 90 days (run via cron)
-- DELETE FROM telemetry WHERE ts < now() - interval '90 days';
-- DELETE FROM audit_log WHERE ts < now() - interval '365 days';
