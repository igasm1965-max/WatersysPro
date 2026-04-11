#!/bin/bash
# PostgreSQL backup script for WatersysPro
# Usage: ./scripts/backup_db.sh [output_dir]
# Cron example: 0 3 * * * /path/to/backup_db.sh /backups >> /var/log/db_backup.log 2>&1

set -euo pipefail

BACKUP_DIR="${1:-./backups}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="${BACKUP_DIR}/watersys_${TIMESTAMP}.sql.gz"
RETAIN_DAYS="${RETAIN_DAYS:-30}"

# Load env if available
[ -f .env ] && export $(grep -v '^#' .env | xargs)

DB_HOST="${PGHOST:-localhost}"
DB_PORT="${PGPORT:-5432}"
DB_USER="${PGUSER:-postgres}"
DB_NAME="${PGDATABASE:-watersys}"

mkdir -p "$BACKUP_DIR"

echo "[$(date)] Starting backup of ${DB_NAME}@${DB_HOST}:${DB_PORT}..."

# Use docker exec if running in compose, or pg_dump directly
if command -v docker &>/dev/null && docker ps --format '{{.Names}}' | grep -q postgres; then
  docker exec -e PGPASSWORD="${PGPASSWORD:-}" $(docker ps -qf "name=postgres") \
    pg_dump -U "$DB_USER" -d "$DB_NAME" --format=custom | gzip > "$BACKUP_FILE"
else
  PGPASSWORD="${PGPASSWORD:-}" pg_dump -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" \
    -d "$DB_NAME" --format=custom | gzip > "$BACKUP_FILE"
fi

FILESIZE=$(stat -c%s "$BACKUP_FILE" 2>/dev/null || stat -f%z "$BACKUP_FILE" 2>/dev/null || echo "?")
echo "[$(date)] Backup complete: ${BACKUP_FILE} (${FILESIZE} bytes)"

# Upload to S3 if AWS CLI is available
if command -v aws &>/dev/null && [ -n "${S3_BACKUP_BUCKET:-}" ]; then
  aws s3 cp "$BACKUP_FILE" "s3://${S3_BACKUP_BUCKET}/db-backups/$(basename $BACKUP_FILE)"
  echo "[$(date)] Uploaded to s3://${S3_BACKUP_BUCKET}/db-backups/"
fi

# Cleanup old backups
find "$BACKUP_DIR" -name "watersys_*.sql.gz" -mtime +${RETAIN_DAYS} -delete
echo "[$(date)] Cleaned up backups older than ${RETAIN_DAYS} days"
