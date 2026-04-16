'use client';
import { createTheme } from '@mui/material/styles';
import { ThemeOptions } from '@mui/material/styles';

export const themeOptions: ThemeOptions = {
  cssVariables: true,
  palette: {
    mode: 'dark',
    primary: {
      main: '#e0d0f7',
      contrastText: '#0e0725',
    },
    secondary: {
      main: '#c383e2',
      contrastText: '#1b0e46',
    },
    background: {
      default: '#191429',
      paper: '#545481',
    },
    divider: 'rgba(177,160,195,0.5)',
    text: {
      primary: '#ffffff',
    },
    warning: {
      main: '#ffb420',
    },
    error: {
      main: '#ec4338',
    },
    info: {
      main: '#d076f3',
    },
    success: {
      main: '#4fce53',
    },
  },
  typography: {
    fontFamily: '"Geist", "Geist Mono", "Helvetica", "Arial", sans-serif',
  },
  spacing: 8,
  shape: {
    borderRadius: 4,
  },
};

const theme = createTheme(themeOptions);

export default theme;
