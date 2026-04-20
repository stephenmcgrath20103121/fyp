'use client';
import { createTheme } from '@mui/material/styles';
import { ThemeOptions } from '@mui/material/styles';

declare module '@mui/material/styles' {
  interface Palette {
    tertiary: Palette['primary'];
    video: Palette['primary'];
    music: Palette['primary'];
    image: Palette['primary'];
  }

  interface PaletteOptions {
    tertiary?: PaletteOptions['primary'];
    video?: PaletteOptions['primary'];
    music?: PaletteOptions['primary'];
    image?: PaletteOptions['primary'];
  }
}

declare module '@mui/material/Avatar' {
  interface AvatarPropsColorOverrides {
    video: true;
    music: true;
    image: true;
  }
}

declare module '@mui/material/ListItemButton' {
  interface ListItemButtonPropsColorOverrides {
    video: true;
    music: true;
    image: true;
  }
}

declare module '@mui/material/Card' {
  interface ListItemButtonPropsColorOverrides {
    tertiary: true;
  }
}

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
    tertiary: {
      main: '#8580b8',
      light: '#b7b1fd',
      dark: '#5f5c84',
      contrastText: '#0e0725',
    },
    video: {
      main: '#ea8706',
      light: '#f1cdb9',
      dark: '#8b5104',
      contrastText: '#0e0725',
    },
    music: {
      main: '#abf353',
      light: '#ecfadb',
      dark: '#599b09',
      contrastText: '#0e0725',
    },
    image: {
      main: '#30deea',
      light: '#d5f8fb',
      dark: '#0e8890',
      contrastText: '#0e0725',
    },
  },
  typography: {
    fontFamily: '"Geist", "Geist Mono", "Roboto", "Helvetica", "Arial", sans-serif',
  },
  spacing: 8,
  shape: {
    borderRadius: 4,
  },
};

const theme = createTheme(themeOptions);

export default theme;
