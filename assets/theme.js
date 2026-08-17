(function() {
  'use strict';

  const preferences = ['system', 'light', 'dark'];
  const systemPreference = 'system';
  const darkScheme = window.matchMedia('(prefers-color-scheme: dark)');
  let preference = null;

  function normalizePreference(value) {
    if (preferences.includes(value)) return value;
    const index = Number(value);
    return Number.isInteger(index) && preferences[index] ?
        preferences[index] : systemPreference;
  }

  function resolvedTheme() {
    if (preference !== systemPreference) return preference;
    return darkScheme.matches ? 'dark' : 'light';
  }

  function applyTheme() {
    const theme = resolvedTheme();
    document.documentElement.dataset.theme = theme;
    const themeColor = document.querySelector('meta[name="theme-color"]');
    if (themeColor)
      themeColor.setAttribute(
          'content', theme === 'dark' ? '#0b1020' : '#f8fafc');
  }

  window.setThemePreference = function(value) {
    preference = normalizePreference(value);
    applyTheme();
    const select = document.getElementById('theme');
    if (select) select.value = preferences.indexOf(preference).toString();
  };

  if (darkScheme.addEventListener)
    darkScheme.addEventListener('change', function() {
      if (preference === systemPreference) applyTheme();
    });
  else
    darkScheme.addListener(function() {
      if (preference === systemPreference) applyTheme();
    });
})();
