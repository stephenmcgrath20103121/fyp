'use client'
import * as React from 'react';
import Grid from '@mui/material/Grid';
import Typography from '@mui/material/Typography';
import ForestIcon from '@mui/icons-material/Forest';
import Avatar from '@mui/material/Avatar';

export default function Options() {
  return (
    <Grid sx={{maxWidth: 350, bgcolor: 'background.paper', mb: 1, p: 1, borderRadius: 3}}>
    <Grid container sx={{ maxWidth: 340, justifyContent: 'center', bgcolor: 'tertiary.main', mb: 1, padding: 1, borderRadius: 3}}>
      <Grid container size="grow" sx={{ minWidth: 175, justifyContent: 'space-evenly', maxWidth: 340, margin: 'auto', pt: 1, pb: 1, pl: 1, pr: 4, color:'tertiary.contrastText' }}>
        <Avatar sx={{width: 64, height: 64, bgcolor: 'forestgreen'}}>
          <ForestIcon sx={{width: 36, height: 36, color: 'primary.contrastText'}} />
        </Avatar>
      <Typography component ="h4" variant="h4" gutterBottom sx={{pt:1, color: 'text.primary'}}>
        MediaRoot
      </Typography>
      </Grid>
    </Grid>
    </Grid>
  );
}