/**
 * @file theme.ts
 * @brief Централизованная цветовая палитра и отступы приложения.
 *
 * Единый источник цветов/отступов вместо инлайн-строк в экранах.
 */
export const colors = {
  // Бренд / акцент
  primary: '#2196F3',

  // Зелёный (успех / бак 2 / активные этапы)
  success: '#2e7d32',
  successDark: '#1b5e20',
  successLight: '#43a047',
  successBg: '#e8f5e9',

  // Красный (авария / опасность)
  danger: '#c62828',
  dangerLight: '#ef4444',
  dangerDark: '#b91c1c',
  dangerBg: '#ffebee',

  // Синий (бак 1 / информация)
  info: '#1565c0',

  // Индикатор подключения
  ok: '#22c55e',
  okDark: '#15803d',

  // Нейтральные
  background: '#f5f5f5',
  card: '#ffffff',
  border: '#e0e0e0',
  ink: '#1e293b',
  slate: '#64748b',
  textPrimary: '#333333',
  textSecondary: '#666666',
  textLight: '#757575',
  textMuted: '#9e9e9e',
  textFaint: '#aaaaaa',
  white: '#ffffff',
};

export const spacing = {
  xs: 4,
  sm: 8,
  md: 16,
  lg: 24,
};

export const radius = {
  sm: 4,
  md: 8,
  pill: 20,
};